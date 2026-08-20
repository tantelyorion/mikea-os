#include "graphics.h"

#include "../../../assets/fonts/font8x8_basic.h"


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


gfx_ready = 1;


console_write("[graphics] Mode graphique actif\n");


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


u8* fb = (u8*)(u64)vbe_info->addr;

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


for (u32 j = 0; j < h; j++)
{

for (u32 i = 0; i < w; i++)
{

gfx_put_pixel(x + i, y + j, color);

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


u8* fb = (u8*)(u64)vbe_info->addr;

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

for (u32 i = 0; i < total_bytes - row_bytes; i++)
{

fb[i] = fb[i + row_bytes];

}


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

u8* fb = (u8*)(u64)vbe_info->addr;


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


u32 bytes_per_pixel = vbe_info->bpp / 8;

u8* fb = (u8*)(u64)vbe_info->addr;


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


u8* pixel = fb + ((y + row) * vbe_info->pitch) + ((x + col) * bytes_per_pixel);


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


}


}


void gfx_write_rect(u32 x, u32 y, u32 w, u32 h, const u8* buffer)
{


if (!gfx_ready)
{

return;

}


u32 bytes_per_pixel = vbe_info->bpp / 8;

u8* fb = (u8*)(u64)vbe_info->addr;


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
