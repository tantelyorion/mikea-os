#include "console.h"

#include "../drivers/framebuffer.h"

#include "../drivers/graphics/graphics.h"



static int cursor_x=0;
static int cursor_y=0;


/*
    ============================================================
    Correctif (etape 2 interface graphique) : bascule pixels
    ============================================================

    Toutes les fonctions ci-dessous verifient gfx_available()
    et delegatent au rendu pixel (police 8x8, voir
    kernel/drivers/graphics) quand un mode graphique reel a ete
    active par boot/loader/stage2.asm. Aucun appelant existant
    (login.c, commands.c, msh.c, kernel.c...) n'a besoin d'etre
    modifie : ils continuent d'appeler console_write() comme
    avant, dans les deux cas.

    GFX_SCALE=2 donne des caracteres 16x16 pixels (police 8x8
    agrandie), plus lisibles sur un ecran haute resolution que
    des caracteres 8x8 natifs minuscules.
*/

#define GFX_SCALE 2

#define GFX_FG GFX_CYAN

#define GFX_BG GFX_DARKBLUE


static u32 gfx_cols()
{

return gfx_width() / (8 * GFX_SCALE);

}


static u32 gfx_rows()
{

return gfx_height() / (8 * GFX_SCALE);

}


void console_init()
{

cursor_x=0;
cursor_y=0;


if (gfx_available())
{

gfx_clear(GFX_BG);

}

}



void console_write(
const char* text
)
{


if (gfx_available())
{


while(*text)
{


if(*text=='\n')
{

cursor_x=0;
cursor_y++;

text++;

if (cursor_y >= (int)gfx_rows())
{

gfx_scroll_up(8 * GFX_SCALE, GFX_BG);

cursor_y = (int)gfx_rows() - 1;

}

continue;

}


gfx_draw_char(
(u32)cursor_x * 8 * GFX_SCALE,
(u32)cursor_y * 8 * GFX_SCALE,
*text,
GFX_FG,
GFX_SCALE
);


cursor_x++;

text++;


if (cursor_x >= (int)gfx_cols())
{

cursor_x = 0;

cursor_y++;

if (cursor_y >= (int)gfx_rows())
{

gfx_scroll_up(8 * GFX_SCALE, GFX_BG);

cursor_y = (int)gfx_rows() - 1;

}

}


}


return;

}



while(*text)
{


if(*text=='\n')
{

cursor_x=0;
cursor_y++;

text++;

if (cursor_y >= FB_HEIGHT)
{

fb_scroll();

cursor_y = FB_HEIGHT - 1;

}

continue;

}


/*
    Correctif : la version precedente appelait
    fb_write() (qui reecrit toute la chaine restante)
    a chaque caractere, soit un travail O(n^2) pour
    une chaine de longueur n. On ecrit ici un seul
    caractere a la fois avec fb_put(), en O(n).
*/

fb_put(
cursor_x,
cursor_y,
*text,
0x0F
);


cursor_x++;

text++;


/*
    Correctif : cursor_x/cursor_y n'etaient jamais
    bornes a la grille VGA (80x25). Avec plus de 25
    lignes affichees (le boot a lui seul en ecrit plus
    de 50), cursor_y depassait largement FB_HEIGHT et
    fb_put() ecrivait hors de la memoire video (voir
    framebuffer.c). On enveloppe desormais la ligne a
    FB_WIDTH et on fait defiler l'ecran a FB_HEIGHT.
*/

if (cursor_x >= FB_WIDTH)
{

cursor_x = 0;

cursor_y++;

if (cursor_y >= FB_HEIGHT)
{

fb_scroll();

cursor_y = FB_HEIGHT - 1;

}

}


}


}



void console_backspace()
{

/*
    Recule le curseur d'une position (y compris en
    remontant d'une ligne si on est en debut de ligne),
    puis efface le caractere qui s'y trouvait.
*/

if (gfx_available())
{


if (cursor_x == 0)
{

if (cursor_y == 0)
{

return;

}

cursor_y--;

cursor_x = (int)gfx_cols() - 1;

}
else
{

cursor_x--;

}


gfx_fill_rect(
(u32)cursor_x * 8 * GFX_SCALE,
(u32)cursor_y * 8 * GFX_SCALE,
8 * GFX_SCALE,
8 * GFX_SCALE,
GFX_BG
);


return;

}


if (cursor_x == 0)
{

if (cursor_y == 0)
{

/* Deja tout en haut a gauche : rien a effacer. */

return;

}

cursor_y--;

cursor_x = FB_WIDTH - 1;

}
else
{

cursor_x--;

}


fb_put(
cursor_x,
cursor_y,
' ',
0x0F
);

}



void console_clear()
{


if (gfx_available())
{

gfx_clear(GFX_BG);

cursor_x = 0;

cursor_y = 0;

return;

}


fb_clear();

cursor_x = 0;

cursor_y = 0;

}
