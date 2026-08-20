#include "settings.h"

#include "../session/session.h"
#include "../../gui/gui.h"

#include "../../gui/theme.h"
#include "../../kernel/drivers/graphics/graphics.h"
#include "../../kernel/drivers/mouse/mouse.h"
#include "../../shell/msh.h"


void console_write(const char* text);
void console_clear();
void keyboard_flush();
int keyboard_available();
char keyboard_getchar();
unsigned long timer_ticks();


void cmd_settings()
{


if (!gfx_available())
{

console_write("Les parametres graphiques necessitent le mode graphique et une souris.\n");

return;

}


int win_x = 2, win_y = 2, win_w = 34, win_h = 20;


u32 logout_x, logout_y, logout_w, logout_h;

u32 theme_btn_x, theme_btn_y, theme_btn_w, theme_btn_h;

u32 close_bx, close_by, close_bsize;


keyboard_flush();


int was_pressed = 0;

int redraw = 1;

gui_drag drag;

drag.active = 0;


while (1)
{


/*
    Le theme (clair/sombre) peut changer pendant que cette
    fenetre est ouverte -- voir plus bas, clic sur le bouton
    "Theme". "redraw" force alors un nouveau passage complet
    (console_clear() efface tout l'ecran avec le fond du
    NOUVEAU theme, sans quoi l'ancien fond resterait visible
    autour de la fenetre redessinee). Sert aussi apres un
    glisser-deposer de la fenetre (voir plus bas), win_x/win_y
    ayant alors change.
*/

if (redraw)
{


console_clear();

gui_draw_window(win_x, win_y, win_w, win_h, "Parametres", theme_text_attr(), &close_bx, &close_by, &close_bsize);

gui_draw_session_content(win_x, win_y);

gui_draw_button(win_x + 1, win_y + win_h - 6, win_w - 2, 2, "Deconnexion", &logout_x, &logout_y, &logout_w, &logout_h);

gui_draw_button(
win_x + 1, win_y + win_h - 4, win_w - 2, 2,
theme_is_dark() ? "Theme : Sombre" : "Theme : Clair",
&theme_btn_x, &theme_btn_y, &theme_btn_w, &theme_btn_h
);

gui_draw_text(win_x + 1, win_y + win_h - 1, "X ou Entree pour fermer -- barre de titre pour deplacer", theme_text_attr());


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


if (clicked && !drag.active && gui_point_in_rect(logout_x, logout_y, logout_w, logout_h, mx, my))
{

gui_cursor_erase();

console_clear();

shell_request_logout();

return;

}


if (clicked && !drag.active && gui_point_in_rect(theme_btn_x, theme_btn_y, theme_btn_w, theme_btn_h, mx, my))
{

theme_toggle_dark_mode();

redraw = 1;

}


if (keyboard_available())
{

char c = keyboard_getchar();

if (c == '\n')
{

break;

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
