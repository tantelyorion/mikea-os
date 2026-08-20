#include "icons.h"


/*
    Arithmetique entiere uniquement partout ci-dessous (pas de
    flottants) : ce noyau est compile avec -mno-sse/-mno-sse2 et
    n'a pas de logique de repli "soft-float" testee -- comme le
    reste du projet (voir kernel/), on reste sur u32/s32 avec des
    divisions/multiplications entieres, quitte a arrondir les
    proportions des icones plutot que d'introduire le premier
    calcul flottant du noyau pour un simple habillage visuel.
*/


void icon_draw_folder(u32 px, u32 py, u32 size, gfx_color color)
{

u32 tab_w = size * 2 / 5;

u32 tab_h = size / 6;

if (tab_h < 1) { tab_h = 1; }


gfx_fill_rect(px, py, tab_w, tab_h, color);

gfx_fill_rect(px, py + tab_h, size, size - tab_h, color);

}


void icon_draw_calculator(u32 px, u32 py, u32 size, gfx_color color)
{

gfx_draw_rect(px, py, size, size, color);


u32 margin = size / 6;

u32 screen_h = size / 4;

if (screen_h < 1) { screen_h = 1; }


gfx_fill_rect(px + margin, py + margin, size - 2 * margin, screen_h, color);


u32 grid_y = py + margin + screen_h + margin / 2 + 1;

u32 cell = size / 6;

if (cell < 1) { cell = 1; }

u32 gap = cell / 2;

if (gap < 1) { gap = 1; }


for (u32 row = 0; row < 2; row++)
{

for (u32 col = 0; col < 3; col++)
{

u32 cx = px + margin + col * (cell + gap);

u32 cy = grid_y + row * (cell + gap);


if (cx + cell <= px + size - margin / 2 && cy + cell <= py + size - margin / 2)
{

gfx_fill_rect(cx, cy, cell, cell, color);

}

}

}

}


void icon_draw_terminal(u32 px, u32 py, u32 size, gfx_color color)
{

gfx_draw_rect(px, py, size, size, color);

if (size > 3)
{

gfx_draw_rect(px + 1, py + 1, size - 2, size - 2, color);

}


/*
    Invite ">_" via la police bitmap existante
    (assets/fonts/font8x8_basic.h) plutot que de redessiner un
    chevron a coups de rectangles : plus simple, et c'est deja
    le symbole universellement reconnu d'un terminal.
*/

if (size >= 16)
{

gfx_draw_text(px + size / 6, py + (size - 8) / 2, ">_", color, 1);

}

}


void icon_draw_settings(u32 px, u32 py, u32 size, gfx_color color)
{

u32 line_x0 = px + size / 8;

u32 line_w = size - size / 4;

if (line_w < 2) { line_w = 2; }


u32 line_y[3];

line_y[0] = py + size / 6;

line_y[1] = py + size / 2;

line_y[2] = py + (size * 5) / 6;


u32 handle_size = size / 6;

if (handle_size < 2) { handle_size = 2; }


/* Position horizontale de la poignee, en alternance gauche/centre/droite. */

u32 handle_x[3];

handle_x[0] = line_x0 + line_w / 4;

handle_x[1] = line_x0 + line_w / 2;

handle_x[2] = line_x0 + (line_w * 3) / 4;


for (int i = 0; i < 3; i++)
{

gfx_draw_hline(line_x0, line_y[i], line_w, color);


u32 hx = (handle_x[i] > handle_size / 2) ? handle_x[i] - handle_size / 2 : 0;

u32 hy = (line_y[i] > handle_size / 2) ? line_y[i] - handle_size / 2 : 0;


gfx_fill_rect(hx, hy, handle_size, handle_size, color);

}

}


void icon_draw_user(u32 px, u32 py, u32 size, gfx_color color)
{

u32 head_size = size * 2 / 5;

if (head_size < 2) { head_size = 2; }

u32 head_x = px + (size - head_size) / 2;

u32 head_y = py + size / 10;


gfx_fill_rect(head_x, head_y, head_size, head_size, color);


u32 shoulders_y = head_y + head_size + size / 12;

u32 shoulders_h = (shoulders_y < py + size) ? (py + size - shoulders_y) : 1;

u32 shoulders_w = (size * 4) / 5;

u32 shoulders_x = px + (size - shoulders_w) / 2;


gfx_fill_rect(shoulders_x, shoulders_y, shoulders_w, shoulders_h, color);

}


void icon_draw_power(u32 px, u32 py, u32 size, gfx_color color)
{

/*
    Pas de primitive de cercle dans kernel/drivers/graphics :
    un carre encadre tient lieu d'anneau (meme convention
    blocs/pixel-art que les autres icones ci-dessus), avec un
    trait vertical qui en depasse par le haut -- silhouette du
    symbole d'alimentation universel malgre tout reconnaissable
    a cette taille.
*/

u32 ring_size = (size * 7) / 10;

u32 ring_x = px + (size - ring_size) / 2;

u32 ring_y = py + size / 6;


gfx_draw_rect(ring_x, ring_y, ring_size, ring_size, color);

if (ring_size > 3)
{

gfx_draw_rect(ring_x + 1, ring_y + 1, ring_size - 2, ring_size - 2, color);

}


u32 line_x = px + size / 2;

gfx_draw_vline(line_x, py, size / 3, color);

}


void icon_draw_grid(u32 px, u32 py, u32 size, gfx_color color)
{

u32 dot = size / 6;

if (dot < 2) { dot = 2; }

u32 gap = size / 8;

if (gap < 1) { gap = 1; }


u32 total = 3 * dot + 2 * gap;

u32 offset = (size > total) ? (size - total) / 2 : 0;


for (u32 row = 0; row < 3; row++)
{

for (u32 col = 0; col < 3; col++)
{

u32 x = px + offset + col * (dot + gap);

u32 y = py + offset + row * (dot + gap);

gfx_fill_rect(x, y, dot, dot, color);

}

}

}
