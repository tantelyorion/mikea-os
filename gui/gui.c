#include "gui.h"

#include "../kernel/drivers/framebuffer.h"



void gui_draw_text(int x, int y, const char* text, u8 color)
{

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



void gui_draw_window(int x, int y, int width, int height, const char* title, u8 color)
{

gui_draw_box(x, y, width, height, color);


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
