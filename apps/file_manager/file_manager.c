#include "file_manager.h"

#include "../../gui/gui.h"

#include "../../gui/theme.h"
#include "../../kernel/drivers/graphics/graphics.h"
#include "../../kernel/drivers/mouse/mouse.h"
#include "../../filesystem/inode.h"
#include "../../filesystem/file.h"


void console_write(const char* text);
void console_clear();
void keyboard_flush();
int keyboard_available();
char keyboard_getchar();
unsigned long timer_ticks();


void cmd_files()
{


if (!gfx_available())
{

console_write("L'explorateur de fichiers necessite le mode graphique et une souris.\n");

return;

}


int win_x = 2, win_y = 2, win_w = 34, win_h = 20;


/* Jusqu'a 12 fichiers listes a l'ecran (au-dela, non affiches -- limite simple). */

char names[12][FILE_NAME_SIZE];

u32 name_count = 0;


for (u32 id = 1; id <= MAX_INODES && name_count < 12; id++)
{

inode* node = inode_get(id);

if (node != 0)
{

int i = 0;

while (node->name[i] != 0 && i < FILE_NAME_SIZE - 1)
{

names[name_count][i] = node->name[i];

i++;

}

names[name_count][i] = 0;

name_count++;

}

}


int viewing = -1; /* -1 = liste, sinon index dans names[] */

u32 row_x[12], row_y[12], row_w[12], row_h[12];

u32 close_bx = 0, close_by = 0, close_bsize = 0;

char* file_content = (void*)0;


keyboard_flush();


int redraw = 1;

int was_pressed = 0;

gui_drag drag;

drag.active = 0;


while (1)
{


if (redraw)
{


console_clear();


if (viewing < 0)
{

gui_draw_window(win_x, win_y, win_w, win_h, "Explorateur de fichiers", theme_text_attr(), &close_bx, &close_by, &close_bsize);


if (name_count == 0)
{

gui_draw_text(win_x + 1, win_y + 2, "(aucun fichier)", theme_text_attr());

}


for (u32 i = 0; i < name_count; i++)
{

gui_draw_button(win_x + 1, win_y + 2 + (int)i, win_w - 2, 1, names[i], &row_x[i], &row_y[i], &row_w[i], &row_h[i]);

}


gui_draw_text(win_x + 1, win_y + win_h - 2, "Cliquez un fichier -- X ou Entree pour fermer", theme_text_attr());

}
else
{

gui_draw_window(win_x, win_y, win_w, win_h, names[viewing], theme_text_attr(), &close_bx, &close_by, &close_bsize);


file_content = file_read(names[viewing]);

if (file_content != (void*)0)
{

gui_draw_text(win_x + 1, win_y + 2, file_content, theme_text_attr());

}
else
{

gui_draw_text(win_x + 1, win_y + 2, "(impossible de lire ce fichier)", theme_text_attr());

}


gui_draw_text(win_x + 1, win_y + win_h - 2, "Clic ou Entree pour revenir a la liste", theme_text_attr());

}


redraw = 0;

}


s32 mx = mouse_get_x();

s32 my = mouse_get_y();

gui_draw_cursor(mx, my);


int now_pressed = mouse_left_pressed();

int clicked = (now_pressed && !was_pressed);

was_pressed = now_pressed;


if (gui_drag_update(&drag, &win_x, &win_y, win_w, win_h, mx, my, now_pressed, close_bx, close_by, close_bsize))
{

redraw = 1;

}


if (clicked && gui_point_in_rect(close_bx, close_by, close_bsize, close_bsize, mx, my))
{

break;

}


if (clicked && !drag.active)
{

if (viewing < 0)
{

for (u32 i = 0; i < name_count; i++)
{

if (gui_point_in_rect(row_x[i], row_y[i], row_w[i], row_h[i], mx, my))
{

viewing = (int)i;

redraw = 1;

break;

}

}

}
else
{

viewing = -1;

redraw = 1;

}

}


if (keyboard_available())
{

char c = keyboard_getchar();

if (c == '\n')
{

if (viewing < 0)
{

break;

}

else
{

viewing = -1;

redraw = 1;

}

}

}


unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{
}


}


gui_cursor_erase();

console_clear();


}