#include "gui.h"

#include "../kernel/drivers/framebuffer.h"

#include "../kernel/drivers/graphics/graphics.h"


/*
    Correctif (fenetre invisible en mode graphique) : ces
    fonctions ecrivaient directement dans la memoire texte VGA
    (0xB8000) via fb_put() -- sans effet une fois qu'un vrai
    mode graphique pixels est actif (voir
    kernel/console/console.c pour le meme correctif applique a
    la console). On bascule ici vers les primitives pixel
    (kernel/drivers/graphics) des que gfx_available() est vrai,
    avec les memes conventions de coordonnees "cellule de texte"
    et le meme facteur d'echelle (GFX_SCALE=2) que console.c,
    pour que "gui" s'aligne visuellement avec le reste de
    l'affichage.
*/

#define GFX_SCALE 2

#define GUI_GFX_FG GFX_CYAN

#define GUI_GFX_BG 0x1B3A5C

#define GUI_GFX_TITLE_FG GFX_WHITE


static void gui_draw_text_gfx(int x, int y, const char* text)
{

gfx_draw_text(
(u32)x * 8 * GFX_SCALE,
(u32)y * 8 * GFX_SCALE,
text,
GUI_GFX_FG,
GFX_SCALE
);

}


void gui_draw_text(int x, int y, const char* text, u8 color)
{


if (gfx_available())
{

gui_draw_text_gfx(x, y, text);

return;

}


int i = 0;

while (text[i] != 0)
{

fb_put(x + i, y, text[i], color);

i++;

}

}



void gui_draw_box(int x, int y, int width, int height, u8 color)
{

if (width < 2 || height < 2)
{
/* Trop petit pour dessiner une bordure complete. */
return;
}


if (gfx_available())
{


u32 px = (u32)x * 8 * GFX_SCALE;

u32 py = (u32)y * 8 * GFX_SCALE;

u32 pw = (u32)width * 8 * GFX_SCALE;

u32 ph = (u32)height * 8 * GFX_SCALE;


gfx_fill_rect(px, py, pw, ph, GUI_GFX_BG);

gfx_draw_rect(px, py, pw, ph, GUI_GFX_FG);

if (pw > 2 && ph > 2)
{

gfx_draw_rect(px + 1, py + 1, pw - 2, ph - 2, GUI_GFX_FG);

}


return;

}


/* Coins */

fb_put(x, y, '+', color);

fb_put(x + width - 1, y, '+', color);

fb_put(x, y + height - 1, '+', color);

fb_put(x + width - 1, y + height - 1, '+', color);


/* Bordures horizontales */

for (int i = 1; i < width - 1; i++)
{

fb_put(x + i, y, '-', color);

fb_put(x + i, y + height - 1, '-', color);

}


/* Bordures verticales et interieur vide */

for (int j = 1; j < height - 1; j++)
{

fb_put(x, y + j, '|', color);

fb_put(x + width - 1, y + j, '|', color);


for (int i = 1; i < width - 1; i++)
{

fb_put(x + i, y + j, ' ', color);

}

}

}



void gui_draw_window(int x, int y, int width, int height, const char* title, u8 color, u32* bx, u32* by, u32* bsize)
{

gui_draw_box(x, y, width, height, color);


if (gfx_available())
{


u32 px = (u32)x * 8 * GFX_SCALE;

u32 py = (u32)y * 8 * GFX_SCALE;

u32 pw = (u32)width * 8 * GFX_SCALE;


gfx_fill_rect(px, py, pw, 8 * GFX_SCALE, GUI_GFX_FG);


if (title != (void*)0)
{

gfx_draw_text(px + 8 * GFX_SCALE, py, title, GUI_GFX_TITLE_FG, GFX_SCALE);

}


/*
    Bouton de fermeture ('X') : carre dans le coin superieur
    droit de la barre de titre. Dessine en dernier pour
    rester au-dessus du reste de la barre de titre.
*/

u32 button_size = 8 * GFX_SCALE;

u32 button_x = px + pw - button_size;

u32 button_y = py;


gfx_fill_rect(button_x, button_y, button_size, button_size, 0xB33A3A);

gfx_draw_text(button_x, button_y, "X", GFX_WHITE, GFX_SCALE);


if (bx != (void*)0) { *bx = button_x; }

if (by != (void*)0) { *by = button_y; }

if (bsize != (void*)0) { *bsize = button_size; }


return;

}


/*
    Barre de titre : couleur "inversee" (on echange les
    nibbles fond/texte de l'octet couleur VGA) pour la
    distinguer visuellement du reste de la fenetre.
*/

u8 title_color = (u8)((color << 4) | (color >> 4));


for (int i = 1; i < width - 1; i++)
{

fb_put(x + i, y, ' ', title_color);

}


if (title != (void*)0)
{

int max_len = width - 3;

int i = 0;

while (title[i] != 0 && i < max_len)
{

fb_put(x + 1 + i, y, title[i], title_color);

i++;

}

}

}



void gui_draw_cursor(s32 x, s32 y)
{

if (!gfx_available())
{

return;

}


if (x < 0 || y < 0)
{

return;

}


/*
    Petit curseur en forme de fleche simplifiee (triangle
    plein), plus reconnaissable qu'un simple carre. Dessine
    ligne par ligne, chaque ligne plus etroite que la
    precedente.
*/

gfx_color c = GFX_WHITE;

for (u32 row = 0; row < 10; row++)
{

gfx_fill_rect((u32)x, (u32)y + row, 10 - row, 1, c);

}

}



int gui_point_in_button(u32 bx, u32 by, u32 bsize, s32 px, s32 py)
{

if (px < 0 || py < 0)
{

return 0;

}

u32 x = (u32)px;

u32 y = (u32)py;

return (x >= bx && x < bx + bsize && y >= by && y < by + bsize);

}
