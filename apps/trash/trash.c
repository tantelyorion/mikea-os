#include "trash.h"

#include "../../gui/gui.h"

#include "../../gui/theme.h"
#include "../../gui/icons.h"
#include "../../gui/desktop.h"
#include "../../gui/window.h"
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


#define TRASH_MAX_ROWS 12

#define GFX_SCALE 2


#define TRASH_MODE_LIST 0

#define TRASH_MODE_ITEM 1


typedef struct
{

u32 x, y, w, h;

} trash_hitbox;


static u32 trash_scan(char names[][FILE_NAME_SIZE])
{

u32 count = 0;


for (u32 id = 1; id <= MAX_INODES && count < TRASH_MAX_ROWS; id++)
{

inode* node = inode_get(id);


if (node != 0 && node->deleted)
{

int i = 0;

while (node->name[i] != 0 && i < FILE_NAME_SIZE - 1)
{

names[count][i] = node->name[i];

i++;

}

names[count][i] = 0;

count++;

}

}


return count;

}


void cmd_trash_app()
{


if (!gfx_available())
{

console_write("La corbeille graphique necessite le mode graphique et une souris.\n");

return;

}


int slot = gui_window_claim_slot();

if (slot < 0)
{

return;

}


int win_x = 3, win_y = 3, win_w = 30, win_h = 18;


int maximized = 0;

int saved_x = win_x, saved_y = win_y, saved_w = win_w, saved_h = win_h;


char names[TRASH_MAX_ROWS][FILE_NAME_SIZE];

u32 count = trash_scan(names);


int mode = TRASH_MODE_LIST;

int viewing = -1;


trash_hitbox row[TRASH_MAX_ROWS];

trash_hitbox empty_btn;

trash_hitbox restore_btn, delete_perm_btn;

u32 close_bx = 0, close_by = 0, close_bsize = 0;

u32 max_bx = 0, max_by = 0, max_bsize = 0;

u32 min_bx = 0, min_by = 0, min_bsize = 0;


int redraw = 1;

int was_pressed = 0;

gui_drag drag;

drag.active = 0;


while (1)
{


if (!gui_window_has_focus(slot))
{

redraw = 1;

gui_window_idle();

continue;

}


if (redraw)
{


desktop_render_backdrop();


if (mode == TRASH_MODE_LIST)
{


gui_draw_window(
win_x, win_y, win_w, win_h, "Corbeille", theme_text_attr(),
&close_bx, &close_by, &close_bsize,
&max_bx, &max_by, &max_bsize, maximized,
&min_bx, &min_by, &min_bsize
);


if (count == 0)
{

icon_draw_trash((u32)(win_x + win_w / 2 - 1) * 8 * GFX_SCALE, (u32)(win_y + 5) * 8 * GFX_SCALE, 8 * GFX_SCALE, theme_border());

gui_draw_text(win_x + 1, win_y + 8, "La corbeille est vide.", theme_text_attr());

}


for (u32 i = 0; i < count; i++)
{

icon_draw_trash((u32)(win_x + 1) * 8 * GFX_SCALE, (u32)(win_y + 2 + (int)i) * 8 * GFX_SCALE, 8 * GFX_SCALE, theme_text());

gui_draw_button(win_x + 3, win_y + 2 + (int)i, win_w - 4, 1, names[i], &row[i].x, &row[i].y, &row[i].w, &row[i].h);

}


gui_draw_button(win_x + 1, win_y + win_h - 4, win_w - 2, 2, "Vider la corbeille", &empty_btn.x, &empty_btn.y, &empty_btn.w, &empty_btn.h);

}

else /* TRASH_MODE_ITEM */
{


gui_draw_window(
win_x, win_y, win_w, win_h, names[viewing], theme_text_attr(),
&close_bx, &close_by, &close_bsize,
&max_bx, &max_by, &max_bsize, maximized,
&min_bx, &min_by, &min_bsize
);


gui_draw_text(win_x + 1, win_y + 2, "Cet element est dans la corbeille.", theme_text_attr());


gui_draw_button(win_x + 1, win_y + 5, win_w - 2, 2, "Restaurer", &restore_btn.x, &restore_btn.y, &restore_btn.w, &restore_btn.h);

gui_draw_button(win_x + 1, win_y + 8, win_w - 2, 2, "Supprimer definitivement", &delete_perm_btn.x, &delete_perm_btn.y, &delete_perm_btn.w, &delete_perm_btn.h);

}


was_pressed = mouse_left_pressed();

redraw = 0;

}


s32 mx = mouse_get_x();

s32 my = mouse_get_y();

gui_draw_cursor(mx, my);


int now_pressed = mouse_left_pressed();

int clicked = (now_pressed && !was_pressed);

was_pressed = now_pressed;


if (!maximized && gui_drag_update(&drag, &win_x, &win_y, win_w, win_h, mx, my, now_pressed, close_bx, close_by, close_bsize))
{

redraw = 1;

}


if (clicked && gui_point_in_rect(close_bx, close_by, close_bsize, close_bsize, mx, my))
{

gui_cursor_erase();

gui_window_close(slot);

break;

}


else if (clicked && gui_point_in_rect(min_bx, min_by, min_bsize, min_bsize, mx, my))
{

gui_cursor_erase();

gui_window_minimize(slot);

continue;

}


else if (clicked && gui_point_in_rect(max_bx, max_by, max_bsize, max_bsize, mx, my))
{

if (!maximized)
{

saved_x = win_x; saved_y = win_y; saved_w = win_w; saved_h = win_h;

win_x = 0;

win_y = 0;

win_w = (int)(gfx_width() / (8 * GFX_SCALE));

win_h = (int)(gfx_height() / (8 * GFX_SCALE)) - 2;

maximized = 1;

}
else
{

win_x = saved_x; win_y = saved_y; win_w = saved_w; win_h = saved_h;

maximized = 0;

}

redraw = 1;

was_pressed = 0;

}


else if (clicked && !drag.active)
{


if (mode == TRASH_MODE_LIST)
{


if (gui_point_in_rect(empty_btn.x, empty_btn.y, empty_btn.w, empty_btn.h, mx, my))
{

file_empty_trash();

count = trash_scan(names);

redraw = 1;

}
else
{

for (u32 i = 0; i < count; i++)
{

if (gui_point_in_rect(row[i].x, row[i].y, row[i].w, row[i].h, mx, my))
{

viewing = (int)i;

mode = TRASH_MODE_ITEM;

redraw = 1;

break;

}

}

}

}

else /* TRASH_MODE_ITEM */
{


if (gui_point_in_rect(restore_btn.x, restore_btn.y, restore_btn.w, restore_btn.h, mx, my))
{

file_restore(names[viewing]);

count = trash_scan(names);

mode = TRASH_MODE_LIST;

redraw = 1;

}
else if (gui_point_in_rect(delete_perm_btn.x, delete_perm_btn.y, delete_perm_btn.w, delete_perm_btn.h, mx, my))
{

file_delete(names[viewing]);

count = trash_scan(names);

mode = TRASH_MODE_LIST;

redraw = 1;

}

}

}


if (keyboard_available())
{

char c = keyboard_getchar();

if (c == '\n')
{

if (mode == TRASH_MODE_LIST)
{

gui_cursor_erase();

gui_window_close(slot);

break;

}

mode = TRASH_MODE_LIST;

redraw = 1;

}

}


unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{
}


}


}
