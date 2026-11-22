#include "graphics.h"

#include "../../../assets/fonts/font8x8_basic.h"
#include "../../../assets/fonts/font_ui.h"


void console_write(
const char* text
);


/*
    Structure deposee par boot/loader/stage2.asm a l'adresse
    physique fixe 0x7E00, decrivant le mode graphique lineaire
    detecte (voir le bloc VBE de stage2.asm pour le detail de
    chaque champ). Cette adresse est identity-mappee (voir la
    table de pages construite par stage2.asm), donc directement
    lisible depuis le noyau C sans configuration supplementaire.
*/

typedef struct
{

u32 addr;

u32 pitch;

u32 width;

u32 height;

u8  bpp;

u8  valid;

u8  red_mask_size;

u8  red_field_position;

u8  green_mask_size;

u8  green_field_position;

u8  blue_mask_size;

u8  blue_field_position;

} __attribute__((packed)) vbe_info_t;


#define VBE_INFO_ADDR 0x7E00


static vbe_info_t* vbe_info = (vbe_info_t*)VBE_INFO_ADDR;

static int gfx_ready = 0;


/*
    ============================================================
    Double buffering (tampon d'ecran en RAM)
    ============================================================

    Avant : chaque primitive ecrivait directement dans la memoire
    video (LFB). Un redessin complet (fond d'ecran, fenetre, widgets)
    etait donc visible etape par etape = scintillement, et chaque
    melange alpha LISAIT la memoire video (tres lente).

    Maintenant : on dessine dans un tampon en RAM ordinaire, puis on
    copie vers l'ecran seulement les lignes modifiees (gfx_present).
    Deux mecanismes garantissent qu'un dessin finit toujours a l'ecran :

    1. gfx_present() explicite, appele une fois par image (voir
       gui_draw_cursor(), que toutes les boucles graphiques appellent).
    2. Filet de securite gfx_tick(), appele par le timer (100 Hz) : si
       plus rien n'a ete dessine pendant un tick ET que rien n'a ete
       affiche, il copie tout l'ecran. Un chemin de code qui ne penserait
       jamais a appeler gfx_present() reste donc visible.

    Securite memoire : le tampon est a une adresse FIXE (32 Mio). Il
    n'est active que si le CMOS annonce assez de RAM pour le contenir
    (gfx_ram_top_bytes) ; sinon, ou en cas de doute, le pilote reste en
    ecriture directe, comme avant.
    ============================================================
*/

#define GFX_BACKBUF_ADDR  0x02000000ULL    /* 32 Mio : bien au-dessus du noyau (1 Mio) et de son tas */
#define GFX_BACKBUF_MAX   0x01000000ULL    /* 16 Mio : jusqu'a 1920x1080x4 (8,3 Mo) ou 2560x1440x4 (14,7 Mo) */

static u8* gfx_back = (u8*)0;                       /* NULL = ecriture directe */

static volatile u32 gfx_dirty_y0 = 0xFFFFFFFFu;     /* lignes modifiees depuis le dernier gfx_present */
static volatile u32 gfx_dirty_y1 = 0;
static volatile u32 gfx_seq = 0;                    /* +1 a chaque primitive qui dessine */
static volatile u32 gfx_presented_seq = 0;          /* dernier etat affiche */
static volatile u32 gfx_irq_seen_seq = 0;          /* etat vu au tick precedent (filet de securite) */

static inline void gfx_mark(u32 y0, u32 y1)
{
    if (gfx_back == (u8*)0)
    {
        return;
    }

    if (y0 < gfx_dirty_y0) { gfx_dirty_y0 = y0; }
    if (y1 > gfx_dirty_y1) { gfx_dirty_y1 = y1; }

    gfx_seq++;
}


static void gfx_fix_masks();

/* Base de dessin : tampon RAM si actif, sinon memoire video. */

static inline u8* gfx_base(void)
{
    return (gfx_back != (u8*)0) ? gfx_back : (u8*)(u64)vbe_info->addr;
}

/* Copie rapide (rep movsq) : memoire video = bande passante limitee, on evite les boucles C. */

static void gfx_blit(u8* dst, const u8* src, u64 bytes)
{
    u64 quads = bytes / 8;
    u64 rest = bytes & 7;

    asm volatile("cld; rep movsq" : "+D"(dst), "+S"(src), "+c"(quads) : : "memory");
    asm volatile("cld; rep movsb" : "+D"(dst), "+S"(src), "+c"(rest) : : "memory");
}

/*
    Haut de la RAM d'apres le CMOS (registres 0x34/0x35 = RAM au-dessus de
    16 Mio, en blocs de 64 Kio). 0 si la valeur est absente ou douteuse :
    dans ce cas le tampon n'est PAS active.
*/

static u64 gfx_ram_top_bytes(void)
{
    u8 lo, hi;

    asm volatile("outb %0, %1" : : "a"((u8)0x34), "Nd"((u16)0x70));
    asm volatile("inb %1, %0" : "=a"(lo) : "Nd"((u16)0x71));
    asm volatile("outb %0, %1" : : "a"((u8)0x35), "Nd"((u16)0x70));
    asm volatile("inb %1, %0" : "=a"(hi) : "Nd"((u16)0x71));

    u64 above_16m = ((u64)hi << 8 | lo) * 65536ULL;

    if (above_16m == 0)
    {
        return 0;
    }

    return 16ULL * 1024 * 1024 + above_16m;
}

static void gfx_backbuffer_setup(void)
{
    gfx_back = (u8*)0;

    u64 size = (u64)vbe_info->pitch * vbe_info->height;

    if (size == 0 || size > GFX_BACKBUF_MAX)
    {
        return;
    }

    /* Marge de 1 Mio au-dessus du tampon. */

    if (gfx_ram_top_bytes() < GFX_BACKBUF_ADDR + size + 0x100000ULL)
    {
        return;
    }

    u64* probe = (u64*)(u64)GFX_BACKBUF_ADDR;
    u64* probe_end = (u64*)(u64)(GFX_BACKBUF_ADDR + (size & ~7ULL) - 8);

    probe[0] = 0x4D494B4541414141ULL;
    probe_end[0] = 0x4F53424B4D4D4D4DULL;

    if (probe[0] != 0x4D494B4541414141ULL || probe_end[0] != 0x4F53424B4D4D4D4DULL)
    {
        return;
    }

    /* Copie initiale de l'ecran : le tampon demarre identique a ce qui est affiche. */

    gfx_blit((u8*)(u64)GFX_BACKBUF_ADDR, (const u8*)(u64)vbe_info->addr, size);

    gfx_dirty_y0 = 0xFFFFFFFFu;
    gfx_dirty_y1 = 0;
    gfx_back = (u8*)(u64)GFX_BACKBUF_ADDR;
}

/* Affiche les lignes modifiees. A appeler une fois par image. */

void gfx_present(void)
{
    if (gfx_back == (u8*)0 || gfx_presented_seq == gfx_seq)
    {
        return;
    }

    u32 seq = gfx_seq;
    u32 y0 = gfx_dirty_y0;
    u32 y1 = gfx_dirty_y1;

    gfx_dirty_y0 = 0xFFFFFFFFu;
    gfx_dirty_y1 = 0;

    if (y1 > vbe_info->height) { y1 = vbe_info->height; }

    if (y0 < y1)
    {
        u64 offset = (u64)y0 * vbe_info->pitch;

        gfx_blit((u8*)(u64)vbe_info->addr + offset, gfx_back + offset, (u64)(y1 - y0) * vbe_info->pitch);
    }

    gfx_presented_seq = seq;
}

/*
    Filet de securite (timer, 100 Hz). Si le dessin est calme depuis un
    tick complet et que l'etat courant n'a pas ete affiche, copie TOUT
    l'ecran. Ne touche pas aux lignes "sales" gerees par gfx_present()
    (pas de course avec le fil qui dessine : un ecran complet est un
    sur-ensemble de n'importe quel affichage partiel).
*/

void gfx_tick(void)
{
    if (gfx_back == (u8*)0)
    {
        return;
    }

    u32 seq = gfx_seq;

    if (seq != gfx_irq_seen_seq)
    {
        gfx_irq_seen_seq = seq;     /* ca dessine encore : on attend le calme */
        return;
    }

    if (seq == gfx_presented_seq)
    {
        return;                      /* deja tout affiche */
    }

    gfx_blit((u8*)(u64)vbe_info->addr, gfx_back, (u64)vbe_info->pitch * vbe_info->height);
    gfx_presented_seq = seq;
}

/*
    Repasse en ecriture directe apres avoir tout affiche. Pour les chemins
    ou le timer ne tournera plus (panique du noyau, arret) : le message
    doit etre ecrit directement dans la memoire video pour rester visible.
*/

void gfx_set_direct(void)
{
    if (gfx_back == (u8*)0)
    {
        return;
    }

    u8* back = gfx_back;

    gfx_back = (u8*)0;     /* d'abord : plus personne n'ecrit dans le tampon */
    gfx_blit((u8*)(u64)vbe_info->addr, back, (u64)vbe_info->pitch * vbe_info->height);
}

static u32 gfx_unpack_component(u32 packed, u8 field_position, u8 mask_size);

void graphics_init()
{


if (vbe_info->valid != 1)
{

console_write("[graphics] VBE non disponible (detecte au demarrage) -- mode texte conserve\n");

gfx_ready = 0;

return;

}


/*
    Garde-fou : une profondeur de couleur ou une geometrie
    incoherente (ex. framebuffer non trouve/BIOS defaillant)
    ne doit jamais faire planter le noyau -- on desactive
    simplement le module graphique et on continue en mode
    texte, exactement comme si VBE n'etait pas disponible.
*/

if (vbe_info->bpp != 24 && vbe_info->bpp != 32)
{

console_write("[graphics] Profondeur de couleur non prise en charge -- mode texte conserve\n");

gfx_ready = 0;

return;

}


if (vbe_info->width == 0 || vbe_info->height == 0 || vbe_info->addr == 0)
{

console_write("[graphics] Informations de framebuffer incoherentes -- mode texte conserve\n");

gfx_ready = 0;

return;

}


gfx_fix_masks();

gfx_backbuffer_setup();

gfx_ready = 1;


console_write(gfx_back != (u8*)0
    ? "[graphics] Mode graphique actif (double buffering)\n"
    : "[graphics] Mode graphique actif (ecriture directe)\n");


}


int gfx_available()
{

return gfx_ready;

}


u32 gfx_width()
{

return vbe_info->width;

}


u32 gfx_height()
{

return vbe_info->height;

}


/*
    Empaquete une couleur 0xRRGGBB dans le format natif du
    framebuffer, en utilisant les positions de champ RGB
    relevees au demarrage (voir stage2.asm) plutot que de
    supposer un format fixe -- certains BIOS VBE ne placent pas
    forcement rouge/vert/bleu aux memes bits.
*/

/*
    Correctif (affichage noir/couleurs fausses sous QEMU) : certains
    BIOS VBE ne renseignent pas les champs RedMaskSize/FieldPosition...
    (valeurs a 0). Avec une taille de masque nulle, "8 - size" valait 8 et
    TOUTES les composantes etaient ecrasees a 0 : ecran entierement noir.
    On retombe alors sur le format standard 8:8:8 (BGR en memoire,
    donc rouge en position 16, vert 8, bleu 0).
*/
static void gfx_fix_masks()
{
    if (vbe_info->red_mask_size == 0 || vbe_info->green_mask_size == 0 || vbe_info->blue_mask_size == 0
        || vbe_info->red_mask_size > 8 || vbe_info->green_mask_size > 8 || vbe_info->blue_mask_size > 8)
    {
        vbe_info->red_mask_size = 8;   vbe_info->red_field_position = 16;
        vbe_info->green_mask_size = 8; vbe_info->green_field_position = 8;
        vbe_info->blue_mask_size = 8;  vbe_info->blue_field_position = 0;
    }
}

static u32 gfx_pack_color(gfx_color color)
{


u32 r = (color >> 16) & 0xFF;

u32 g = (color >> 8) & 0xFF;

u32 b = color & 0xFF;


/*
    On ne garde que les bits de poids fort correspondant a la
    taille reelle du champ (ex. un champ de 5 bits ne conserve
    que les 5 bits hauts de la composante 8 bits), comme le
    fait toute conversion RGB888 -> RGB565 classique.
*/

r = r >> (8 - vbe_info->red_mask_size);

g = g >> (8 - vbe_info->green_mask_size);

b = b >> (8 - vbe_info->blue_mask_size);


return (r << vbe_info->red_field_position)
     | (g << vbe_info->green_field_position)
     | (b << vbe_info->blue_field_position);


}


void gfx_put_pixel(u32 x, u32 y, gfx_color color)
{


if (!gfx_ready)
{

return;

}


if (x >= vbe_info->width || y >= vbe_info->height)
{

return;

}


u32 packed = gfx_pack_color(color);


u8* fb = gfx_base();
    gfx_mark(y, y + 1);

u8* pixel = fb + (y * vbe_info->pitch) + (x * (vbe_info->bpp / 8));


pixel[0] = (u8)(packed & 0xFF);

pixel[1] = (u8)((packed >> 8) & 0xFF);

pixel[2] = (u8)((packed >> 16) & 0xFF);


if (vbe_info->bpp == 32)
{

pixel[3] = 0;

}


}


void gfx_fill_rect(u32 x, u32 y, u32 w, u32 h, gfx_color color)
{
    if (!gfx_ready)
    {
        return;
    }

    /*
        Correctif performance (lenteur/scintillement sous QEMU) :
        l'ancienne version appelait gfx_put_pixel() pour CHAQUE pixel,
        recalculant a chaque fois le clipping, l'empaquetage de couleur
        et l'adresse. Un fond d'ecran 1024x768 = ~786 000 appels. On
        decoupe ici le rectangle une seule fois, on empaquete la
        couleur une seule fois, puis on ecrit directement.
    */
    if (x >= vbe_info->width || y >= vbe_info->height || w == 0 || h == 0)
    {
        return;
    }
    if (w > vbe_info->width - x)
    {
        w = vbe_info->width - x;
    }
    if (h > vbe_info->height - y)
    {
        h = vbe_info->height - y;
    }

    u32 packed = gfx_pack_color(color);
    u32 bytes_per_pixel = vbe_info->bpp / 8;
    u8* fb = gfx_base();
    gfx_mark(y, y + h);

    for (u32 j = 0; j < h; j++)
    {
        u8* row = fb + ((u64)(y + j) * vbe_info->pitch) + ((u64)x * bytes_per_pixel);

        if (bytes_per_pixel == 4)
        {
            u32* p32 = (u32*)row;
            for (u32 i = 0; i < w; i++)
            {
                p32[i] = packed;
            }
        }
        else
        {
            for (u32 i = 0; i < w; i++)
            {
                row[i * 3 + 0] = (u8)(packed & 0xFF);
                row[i * 3 + 1] = (u8)((packed >> 8) & 0xFF);
                row[i * 3 + 2] = (u8)((packed >> 16) & 0xFF);
            }
        }
    }
}



void gfx_draw_hline(u32 x, u32 y, u32 length, gfx_color color)
{

for (u32 i = 0; i < length; i++)
{

gfx_put_pixel(x + i, y, color);

}

}


void gfx_draw_vline(u32 x, u32 y, u32 length, gfx_color color)
{

for (u32 j = 0; j < length; j++)
{

gfx_put_pixel(x, y + j, color);

}

}


void gfx_draw_rect(u32 x, u32 y, u32 w, u32 h, gfx_color color)
{

if (w == 0 || h == 0)
{

return;

}

gfx_draw_hline(x, y, w, color);

gfx_draw_hline(x, y + h - 1, w, color);

gfx_draw_vline(x, y, h, color);

gfx_draw_vline(x + w - 1, y, h, color);

}



/* ------------------------------------------------------------
   Texte anti-aliase (police 4 bits/pixel, voir tools/gen_font.py)
   Entiers uniquement : le noyau est compile sans SSE/FPU.
   ------------------------------------------------------------ */

static void gfx_blend_pixel_alpha(u32 x, u32 y, gfx_color color, u32 a)
{
    /* a : 0..255 (couverture du glyphe). 0 = rien, 255 = couleur pleine. */

    if (x >= vbe_info->width || y >= vbe_info->height || a == 0)
    {
        return;
    }

    u32 cr = (color >> 16) & 0xFF;
    u32 cg = (color >> 8) & 0xFF;
    u32 cb = color & 0xFF;

    gfx_mark(y, y + 1);

    u32 bpp = vbe_info->bpp / 8;
    u8* pixel = gfx_base() + ((u64)y * vbe_info->pitch) + ((u64)x * bpp);
    u32 inv = 255 - a;

    if (bpp == 4 && vbe_info->red_field_position == 16 && vbe_info->green_field_position == 8
        && vbe_info->blue_field_position == 0 && vbe_info->red_mask_size == 8
        && vbe_info->green_mask_size == 8 && vbe_info->blue_mask_size == 8)
    {
        u32* p32 = (u32*)pixel;
        u32 e = *p32;
        u32 r = (((e >> 16) & 0xFF) * inv + cr * a + 127) / 255;
        u32 g = (((e >> 8) & 0xFF) * inv + cg * a + 127) / 255;
        u32 b = ((e & 0xFF) * inv + cb * a + 127) / 255;
        *p32 = (r << 16) | (g << 8) | b;
        return;
    }

    u32 existing = pixel[0] | (pixel[1] << 8) | (pixel[2] << 16);
    u32 br = gfx_unpack_component(existing, vbe_info->red_field_position, vbe_info->red_mask_size);
    u32 bg = gfx_unpack_component(existing, vbe_info->green_field_position, vbe_info->green_mask_size);
    u32 bb = gfx_unpack_component(existing, vbe_info->blue_field_position, vbe_info->blue_mask_size);

    u32 r = (br * inv + cr * a + 127) / 255;
    u32 g = (bg * inv + cg * a + 127) / 255;
    u32 b = (bb * inv + cb * a + 127) / 255;

    u32 packed = gfx_pack_color((r << 16) | (g << 8) | b);

    pixel[0] = (u8)(packed & 0xFF);
    pixel[1] = (u8)((packed >> 8) & 0xFF);
    pixel[2] = (u8)((packed >> 16) & 0xFF);

    if (vbe_info->bpp == 32)
    {
        pixel[3] = 0;
    }
}

/* Recherche dichotomique (table triee par point de code). */

static const ui_glyph* gfx_find_glyph(const ui_font* f, u32 cp)
{
    s32 lo = 0;
    s32 hi = (s32)f->count - 1;

    while (lo <= hi)
    {
        s32 mid = (lo + hi) / 2;
        u32 v = f->glyphs[mid].cp;

        if (v == cp)
        {
            return &f->glyphs[mid];
        }

        if (v < cp)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }

    return (const ui_glyph*)0;
}

/*
    Courbe de contraste (gamma ~0.7) : une couverture lineaire rend
    le texte fin et pale sur fond clair ; cette table (niveau 4 bits
    -> alpha 8 bits) donne un trait plus net, comme FreeType/macOS.
*/

static const u8 gfx_aa_curve[16] = { 0, 30, 55, 80, 102, 123, 143, 162, 180, 197, 212, 225, 236, 245, 252, 255 };

static void gfx_draw_glyph_aa(const ui_font* f, const ui_glyph* g, s32 pen_x, s32 baseline_y, gfx_color color)
{
    for (u32 row = 0; row < g->h; row++)
    {
        s32 py = baseline_y + g->yoff + (s32)row;

        if (py < 0)
        {
            continue;
        }

        for (u32 col = 0; col < g->w; col++)
        {
            s32 px = pen_x + g->xoff + (s32)col;

            if (px < 0)
            {
                continue;
            }

            u32 idx = row * g->w + col;
            u8 byte = f->data[g->off + (idx >> 1)];
            u32 level = (idx & 1) ? (byte & 0x0F) : (byte >> 4);

            if (level != 0)
            {
                gfx_blend_pixel_alpha((u32)px, (u32)py, color, gfx_aa_curve[level]);
            }
        }
    }
}

/* Decode un point de code UTF-8 ; avance *p. Invalide -> '?'. */

static u32 gfx_utf8_next(const char** p)
{
    const u8* s = (const u8*)*p;
    u32 c = s[0];

    if (c < 0x80)
    {
        *p += 1;
        return c;
    }

    if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80)
    {
        *p += 2;
        return ((c & 0x1F) << 6) | (s[1] & 0x3F);
    }

    if ((c & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80)
    {
        *p += 3;
        return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }

    *p += 1;
    return '?';
}

u32 gfx_font_height(int font)
{
    return UI_FONTS[font].height;
}

u32 gfx_text_width(const char* text, int font)
{
    const ui_font* f = &UI_FONTS[font];
    u32 width = 0;

    while (*text != 0)
    {
        u32 cp = gfx_utf8_next(&text);
        const ui_glyph* g = gfx_find_glyph(f, cp);

        if (g == (const ui_glyph*)0)
        {
            g = gfx_find_glyph(f, '?');
        }

        if (g != (const ui_glyph*)0)
        {
            width += g->adv;
        }
    }

    return width;
}

static u32 gfx_draw_text_baseline(s32 x, s32 baseline, const char* text, gfx_color color, int font)
{
    if (!gfx_ready)
    {
        return 0;
    }

    const ui_font* f = &UI_FONTS[font];
    s32 pen = x;

    while (*text != 0)
    {
        u32 cp = gfx_utf8_next(&text);
        const ui_glyph* g = gfx_find_glyph(f, cp);

        if (g == (const ui_glyph*)0)
        {
            g = gfx_find_glyph(f, '?');
        }

        if (g == (const ui_glyph*)0)
        {
            continue;
        }

        gfx_draw_glyph_aa(f, g, pen, baseline, color);
        pen += g->adv;
    }

    return (u32)(pen - x);
}

u32 gfx_draw_text_ui(u32 x, u32 y, const char* text, gfx_color color, int font)
{
    return gfx_draw_text_baseline((s32)x, (s32)y + UI_FONTS[font].ascent, text, color, font);
}

/*
    Centre le texte verticalement dans une zone de hauteur "h" en
    s'appuyant sur la hauteur des capitales ('H') : c'est ce que l'oeil
    percoit comme "le milieu" d'une ligne, contrairement a la boite
    ascendante+descendante de la police.
*/

u32 gfx_draw_text_ui_vcenter(u32 x, u32 top, u32 h, const char* text, gfx_color color, int font)
{
    const ui_glyph* cap = gfx_find_glyph(&UI_FONTS[font], 'H');
    s32 cap_h = (cap != (const ui_glyph*)0) ? (s32)cap->h : (s32)UI_FONTS[font].ascent;
    s32 baseline = (s32)top + ((s32)h + cap_h) / 2;

    return gfx_draw_text_baseline((s32)x, baseline, text, color, font);
}


void gfx_draw_char(u32 x, u32 y, char c, gfx_color color, u32 scale)
{


if (!gfx_ready)
{

return;

}


if (scale == 0)
{

scale = 1;

}


u8 code = (u8)c;


if (code > 127)
{

/* Hors de la table (voir assets/fonts/font8x8_basic.h) : rien a dessiner. */

return;

}


/*
    Voie lissee (anti-aliasing) : sur la grille de cellules 16x16
    (scale == 2, celle de tout le bureau/console), on dessine la
    police a chasse fixe lissee centree dans la cellule au lieu
    de la police 8x8 agrandie. Meme cellule, meme avance : aucune
    mise en page existante ne change, mais le rendu n'est plus
    "pixelise". Repli automatique sur la police bitmap sinon.
*/

if (scale == 2 && code >= 32)
{

const ui_font* mono = &UI_FONTS[FONT_MONO];

const ui_glyph* g = gfx_find_glyph(mono, code);

if (g != (const ui_glyph*)0)
{

s32 cell = 8 * (s32)scale;

s32 pen_x = (s32)x + (cell - (s32)g->adv) / 2;

gfx_draw_glyph_aa(mono, g, pen_x, (s32)y + (cell * 13) / 16, color);

return;

}

}


const u8* glyph = font8x8_basic[code];


for (u32 row = 0; row < 8; row++)
{

u8 bits = glyph[row];


for (u32 col = 0; col < 8; col++)
{

if (bits & (1 << col))
{

gfx_fill_rect(x + col * scale, y + row * scale, scale, scale, color);

}

}

}


}


void gfx_draw_text(u32 x, u32 y, const char* text, gfx_color color, u32 scale)
{


if (scale == 0)
{

scale = 1;

}


u32 cursor_x = x;

u32 i = 0;


while (text[i] != 0)
{


if (text[i] == '\n')
{

cursor_x = x;

y += 8 * scale;

i++;

continue;

}


gfx_draw_char(cursor_x, y, text[i], color, scale);


cursor_x += 8 * scale;

i++;


}


}


void gfx_clear(gfx_color color)
{

gfx_fill_rect(0, 0, vbe_info->width, vbe_info->height, color);

}



/*
    Copie vers le haut (destination < source) par blocs de 8 octets
    puis octets restants. Beaucoup plus rapide que la boucle octet par
    octet precedente, qui rendait chaque defilement de console visible
    et saccade sous QEMU (3 Mo copies a chaque ligne).
*/
static void gfx_copy_up(u8* dst, const u8* src, u64 count)
{
    u64 blocks = count / 8;
    u64* d64 = (u64*)dst;
    const u64* s64 = (const u64*)src;
    for (u64 i = 0; i < blocks; i++)
    {
        d64[i] = s64[i];
    }
    for (u64 i = blocks * 8; i < count; i++)
    {
        dst[i] = src[i];
    }
}

void gfx_scroll_up(u32 pixel_rows, gfx_color bg)
{


if (!gfx_ready)
{

return;

}


if (pixel_rows >= vbe_info->height)
{

gfx_clear(bg);

return;

}


u8* fb = gfx_base();

u32 row_bytes = pixel_rows * vbe_info->pitch;

u32 total_bytes = vbe_info->height * vbe_info->pitch;


/*
    Decalage brut memoire (plus rapide que gfx_put_pixel
    pixel par pixel pour deplacer une zone entiere) : on
    copie tout ce qui est en dessous de la bande "pixel_rows"
    vers le haut, puis on efface la bande laissee libre en
    bas. Copie octet par octet (pas de memmove disponible en
    freestanding) mais en ordre croissant, sur une zone qui ne
    se chevauche pas dans ce sens -- correct sans avoir besoin
    d'une copie "a l'envers".
*/

gfx_copy_up(fb, fb + row_bytes, (u64)(total_bytes - row_bytes));

    gfx_mark(0, vbe_info->height);


gfx_fill_rect(0, vbe_info->height - pixel_rows, vbe_info->width, pixel_rows, bg);


}


void gfx_scroll_up_region(u32 top_y, u32 pixel_rows, gfx_color bg)
{


if (!gfx_ready)
{

return;

}


if (top_y >= vbe_info->height)
{

return;

}


u32 region_height = vbe_info->height - top_y;


if (pixel_rows >= region_height)
{

gfx_fill_rect(0, top_y, vbe_info->width, region_height, bg);

return;

}


u8* fb = gfx_base();

u32 row_bytes = pixel_rows * vbe_info->pitch;

u32 region_start = top_y * vbe_info->pitch;

u32 region_bytes = region_height * vbe_info->pitch;


/* Meme technique de copie brute que gfx_scroll_up(), bornee a la bande region_start..fin d'ecran. */

gfx_copy_up(fb + region_start, fb + region_start + row_bytes, (u64)(region_bytes - row_bytes));

    gfx_mark(top_y, vbe_info->height);


gfx_fill_rect(0, vbe_info->height - pixel_rows, vbe_info->width, pixel_rows, bg);


}


u32 gfx_bpp()
{

return vbe_info->bpp;

}


void gfx_read_rect(u32 x, u32 y, u32 w, u32 h, u8* buffer)
{


if (!gfx_ready)
{

return;

}


u32 bytes_per_pixel = vbe_info->bpp / 8;

u8* fb = gfx_base();


for (u32 row = 0; row < h; row++)
{


if (y + row >= vbe_info->height)
{

break;

}


for (u32 col = 0; col < w; col++)
{


if (x + col >= vbe_info->width)
{

continue;

}


u8* src = fb + ((y + row) * vbe_info->pitch) + ((x + col) * bytes_per_pixel);

u8* dst = buffer + (row * w + col) * bytes_per_pixel;


for (u32 b = 0; b < bytes_per_pixel; b++)
{

dst[b] = src[b];

}


}


}


}


/*
    Inverse approximatif de gfx_pack_color() : reconstruit une
    composante 8 bits a partir des bits reellement stockes dans
    le pixel natif. Pour les profondeurs 24/32 bits que ce
    module accepte (voir graphics_init()), chaque champ RGB fait
    generalement 8 bits pile, donc ce n'est pas une simple
    approximation mais une reconstruction exacte.
*/

static u32 gfx_unpack_component(u32 packed, u8 field_position, u8 mask_size)
{

u32 raw = (packed >> field_position) & ((1u << mask_size) - 1u);

return raw << (8 - mask_size);

}


/*
    Facteur commun a gfx_fill_rect_blend()/gfx_fill_rounded_rect_blend()
    ci-dessous : le calcul par pixel (lire le fond, meler avec la
    teinte demandee, reecrire) est identique dans les deux cas --
    seule la FORME de la zone parcourue change (rectangle plein
    contre rectangle a coins arrondis). Extrait ici pour ne pas
    dupliquer cette arithmetique deux fois.
*/

static void gfx_blend_pixel_raw(u32 x, u32 y, u32 tint_r, u32 tint_g, u32 tint_b, u32 alpha_percent)
{


u8* fb = gfx_base();

gfx_mark(y, y + 1);

u32 bytes_per_pixel = vbe_info->bpp / 8;


u8* pixel = fb + ((u64)y * vbe_info->pitch) + ((u64)x * bytes_per_pixel);

    /*
        Voie rapide (amelioration de la fluidite) : en 32 bits avec le
        format standard 8:8:8 (cas de QEMU/SeaBIOS), on melange les trois
        canaux directement sur un mot de 32 bits, sans depaquetage/
        empaquetage generique. Le verre depoli des fenetres redessine
        des centaines de milliers de pixels a chaque affichage : cette
        voie reduit fortement le temps de rendu.
    */
    if (bytes_per_pixel == 4
        && vbe_info->red_field_position == 16 && vbe_info->green_field_position == 8
        && vbe_info->blue_field_position == 0 && vbe_info->red_mask_size == 8
        && vbe_info->green_mask_size == 8 && vbe_info->blue_mask_size == 8)
    {
        u32* p32 = (u32*)pixel;
        u32 e = *p32;
        u32 inv = 100 - alpha_percent;
        u32 r = (((e >> 16) & 0xFF) * inv + tint_r * alpha_percent) / 100;
        u32 g = (((e >> 8) & 0xFF) * inv + tint_g * alpha_percent) / 100;
        u32 b = ((e & 0xFF) * inv + tint_b * alpha_percent) / 100;
        *p32 = (r << 16) | (g << 8) | b;
        return;
    }

    u32 existing = pixel[0] | (pixel[1] << 8) | (pixel[2] << 16);


u32 bg_r = gfx_unpack_component(existing, vbe_info->red_field_position, vbe_info->red_mask_size);

u32 bg_g = gfx_unpack_component(existing, vbe_info->green_field_position, vbe_info->green_mask_size);

u32 bg_b = gfx_unpack_component(existing, vbe_info->blue_field_position, vbe_info->blue_mask_size);


u32 out_r = (bg_r * (100 - alpha_percent) + tint_r * alpha_percent) / 100;

u32 out_g = (bg_g * (100 - alpha_percent) + tint_g * alpha_percent) / 100;

u32 out_b = (bg_b * (100 - alpha_percent) + tint_b * alpha_percent) / 100;


gfx_color blended = (out_r << 16) | (out_g << 8) | out_b;

u32 packed = gfx_pack_color(blended);


pixel[0] = (u8)(packed & 0xFF);

pixel[1] = (u8)((packed >> 8) & 0xFF);

pixel[2] = (u8)((packed >> 16) & 0xFF);


if (vbe_info->bpp == 32)
{

pixel[3] = 0;

}


}


void gfx_fill_rect_blend(u32 x, u32 y, u32 w, u32 h, gfx_color tint, u32 alpha_percent)
{


if (!gfx_ready)
{

return;

}


if (alpha_percent > 100)
{

alpha_percent = 100;

}


u32 tint_r = (tint >> 16) & 0xFF;

u32 tint_g = (tint >> 8) & 0xFF;

u32 tint_b = tint & 0xFF;


for (u32 row = 0; row < h; row++)
{


if (y + row >= vbe_info->height)
{

break;

}


for (u32 col = 0; col < w; col++)
{


if (x + col >= vbe_info->width)
{

continue;

}


gfx_blend_pixel_raw(x + col, y + row, tint_r, tint_g, tint_b, alpha_percent);


}


}


}


/*
    Rectangle "verre depoli" a coins arrondis -- meme rendu que
    gfx_fill_rect_blend() ci-dessus (identique au centre), mais
    exclut de chaque coin un quart de disque de rayon "radius"
    (test de distance au carre, pas de vraie trigonometrie :
    inutile ici, et les fonctions flottantes n'existent pas dans
    ce noyau freestanding). Signature "macOS" (Dock, barre de
    menu superieure -- voir gui/desktop.c) : c'est ce qui
    distingue le plus visuellement ce theme d'un simple decoupage
    Windows/GNOME a angles droits.
*/

void gfx_fill_rounded_rect_blend(u32 x, u32 y, u32 w, u32 h, u32 radius, gfx_color tint, u32 alpha_percent)
{


if (!gfx_ready)
{

return;

}


if (alpha_percent > 100)
{

alpha_percent = 100;

}


/* Un rayon plus grand que la moitie la plus courte du rectangle n'aurait pas de sens (coins qui se chevauchent). */

if (radius * 2 > w) { radius = w / 2; }

if (radius * 2 > h) { radius = h / 2; }


u32 tint_r = (tint >> 16) & 0xFF;

u32 tint_g = (tint >> 8) & 0xFF;

u32 tint_b = tint & 0xFF;


for (u32 row = 0; row < h; row++)
{


if (y + row >= vbe_info->height)
{

break;

}


for (u32 col = 0; col < w; col++)
{


if (x + col >= vbe_info->width)
{

continue;

}


/*
    Determine si (col,row) tombe dans l'un des quatre
    coins EXCLUS (hors du quart de disque de ce coin) --
    en dehors de ces quatre zones, tout le rectangle
    reste plein comme d'habitude.
*/

int in_corner_zone =
(col < radius && row < radius) ||
(col >= w - radius && row < radius) ||
(col < radius && row >= h - radius) ||
(col >= w - radius && row >= h - radius);


if (in_corner_zone)
{


u32 corner_cx = (col < radius) ? radius : w - radius;

u32 corner_cy = (row < radius) ? radius : h - radius;


s32 dx = (s32)col - (s32)corner_cx;

s32 dy = (s32)row - (s32)corner_cy;


u32 dist_sq = (u32)(dx * dx + dy * dy);


if (dist_sq > radius * radius)
{

/* En dehors du quart de disque : pixel du coin, laisse tel quel (pas de fond dessine). */

continue;

}


}


gfx_blend_pixel_raw(x + col, y + row, tint_r, tint_g, tint_b, alpha_percent);


}


}


}


/*
    Disque plein, opaque (PAS de transparence -- utilise pour de
    petits elements d'accent tres visibles, comme les "feux
    tricolores" macOS des barres de titre, voir gui/gui.c). Meme
    test de distance au carre que gfx_fill_rounded_rect_blend()
    ci-dessus, applique a tout le disque plutot qu'a des coins.
*/

void gfx_fill_circle(u32 cx, u32 cy, u32 radius, gfx_color color)
{


if (!gfx_ready)
{

return;

}


for (u32 row = 0; row <= radius * 2; row++)
{


s32 py = (s32)cy - (s32)radius + (s32)row;

if (py < 0 || (u32)py >= vbe_info->height)
{

continue;

}


for (u32 col = 0; col <= radius * 2; col++)
{


s32 px = (s32)cx - (s32)radius + (s32)col;

if (px < 0 || (u32)px >= vbe_info->width)
{

continue;

}


s32 dx = (s32)col - (s32)radius;

s32 dy = (s32)row - (s32)radius;


if ((u32)(dx * dx + dy * dy) > radius * radius)
{

continue;

}


gfx_put_pixel((u32)px, (u32)py, color);


}


}


}


/*
    Remplissage de triangle par balayage de lignes (scanline),
    entiers uniquement (voir la remarque sur -mno-sse/-mno-sse2
    en tete de gui/icons.c) -- utilise pour dessiner le logo
    MikeaOS (voir icon_draw_mikea_logo(), gui/icons.c) sans avoir
    besoin d'un decodeur d'image : le logo est entierement
    compose de triangles.

    Algorithme : pour chaque ligne horizontale y comprise entre
    le haut et le bas du triangle, on calcule les points
    d'intersection de cette ligne avec chacune des 3 aretes (une
    arete horizontale ne contribue aucune intersection), puis on
    remplit le segment [min,max] trouve. Coordonnees signees pour
    accepter des sommets hors-ecran sans deborder.
*/

static s32 gfx_edge_x_at_y(s32 x0, s32 y0, s32 x1, s32 y1, s32 y)
{

if (y1 == y0)
{

return x0;

}

return x0 + (x1 - x0) * (y - y0) / (y1 - y0);

}

void gfx_fill_triangle(
s32 x0, s32 y0,
s32 x1, s32 y1,
s32 x2, s32 y2,
gfx_color color
)
{

if (!gfx_ready)
{

return;

}


s32 y_min = y0;

if (y1 < y_min) { y_min = y1; }

if (y2 < y_min) { y_min = y2; }


s32 y_max = y0;

if (y1 > y_max) { y_max = y1; }

if (y2 > y_max) { y_max = y2; }


for (s32 y = y_min; y <= y_max; y++)
{

if (y < 0 || (u32)y >= vbe_info->height)
{

continue;

}


s32 xs[3];

u32 count = 0;


if ((y0 <= y && y < y1) || (y1 <= y && y < y0))
{

xs[count++] = gfx_edge_x_at_y(x0, y0, x1, y1, y);

}

if ((y1 <= y && y < y2) || (y2 <= y && y < y1))
{

xs[count++] = gfx_edge_x_at_y(x1, y1, x2, y2, y);

}

if ((y2 <= y && y < y0) || (y0 <= y && y < y2))
{

xs[count++] = gfx_edge_x_at_y(x2, y2, x0, y0, y);

}


if (count < 2)
{

continue;

}


s32 x_left = xs[0];

s32 x_right = xs[0];

for (u32 i = 1; i < count; i++)
{

if (xs[i] < x_left) { x_left = xs[i]; }

if (xs[i] > x_right) { x_right = xs[i]; }

}


if (x_left < 0) { x_left = 0; }

if ((u32)x_right >= vbe_info->width) { x_right = (s32)vbe_info->width - 1; }


for (s32 x = x_left; x <= x_right; x++)
{

gfx_put_pixel((u32)x, (u32)y, color);

}

}

}


void gfx_write_rect(u32 x, u32 y, u32 w, u32 h, const u8* buffer)
{


if (!gfx_ready)
{

return;

}


u32 bytes_per_pixel = vbe_info->bpp / 8;

u8* fb = gfx_base();

gfx_mark(y, (y + h > vbe_info->height) ? vbe_info->height : y + h);


for (u32 row = 0; row < h; row++)
{


if (y + row >= vbe_info->height)
{

break;

}


for (u32 col = 0; col < w; col++)
{


if (x + col >= vbe_info->width)
{

continue;

}


u8* dst = fb + ((y + row) * vbe_info->pitch) + ((x + col) * bytes_per_pixel);

const u8* src = buffer + (row * w + col) * bytes_per_pixel;


for (u32 b = 0; b < bytes_per_pixel; b++)
{

dst[b] = src[b];

}


}


}


}

/* ------------------------------------------------------------
   Rectangle a coins arrondis ANTI-ALIASES (entiers uniquement)
   ------------------------------------------------------------ */

/*
    Couverture (0..16) du pixel (i, j) d'un coin de rayon r : 16
    sous-echantillons (grille 4x4) testes contre le cercle de
    centre (r, r). Le coin haut-gauche sert de modele : les trois
    autres coins s'en deduisent par symetrie.
*/

static u32 gfx_corner_coverage(u32 i, u32 j, u32 r)
{
    u32 c = r * 8;
    u32 inside = 0;

    for (u32 sy = 0; sy < 4; sy++)
    {
        for (u32 sx = 0; sx < 4; sx++)
        {
            s32 dx = (s32)(i * 8 + sx * 2 + 1) - (s32)c;
            s32 dy = (s32)(j * 8 + sy * 2 + 1) - (s32)c;

            if ((u32)(dx * dx + dy * dy) <= c * c)
            {
                inside++;
            }
        }
    }

    return inside;
}

void gfx_fill_rounded_rect_aa(u32 x, u32 y, u32 w, u32 h, u32 radius, gfx_color color, u32 alpha_percent)
{
    if (!gfx_ready || w == 0 || h == 0 || alpha_percent == 0)
    {
        return;
    }

    if (alpha_percent > 100)
    {
        alpha_percent = 100;
    }

    u32 r = radius;

    if (r > w / 2) { r = w / 2; }
    if (r > h / 2) { r = h / 2; }

    /* Corps : trois rectangles (opaque -> remplissage direct, sinon melange). */

    #define GFX_BODY(bx, by, bw, bh) \
        do { if ((bw) > 0 && (bh) > 0) { \
            if (alpha_percent == 100) gfx_fill_rect((bx), (by), (bw), (bh), color); \
            else gfx_fill_rect_blend((bx), (by), (bw), (bh), color, alpha_percent); } } while (0)

    GFX_BODY(x + r, y, w - 2 * r, h);
    GFX_BODY(x, y + r, r, h - 2 * r);
    GFX_BODY(x + w - r, y + r, r, h - 2 * r);

    #undef GFX_BODY

    for (u32 j = 0; j < r; j++)
    {
        for (u32 i = 0; i < r; i++)
        {
            u32 cov = gfx_corner_coverage(i, j, r);

            if (cov == 0)
            {
                continue;
            }

            u32 a = (cov * 255 / 16) * alpha_percent / 100;

            gfx_blend_pixel_alpha(x + i,         y + j,         color, a);
            gfx_blend_pixel_alpha(x + w - 1 - i, y + j,         color, a);
            gfx_blend_pixel_alpha(x + i,         y + h - 1 - j, color, a);
            gfx_blend_pixel_alpha(x + w - 1 - i, y + h - 1 - j, color, a);
        }
    }
}

