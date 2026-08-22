#include "settings.h"

#include "../session/session.h"
#include "../../gui/gui.h"

#include "../../gui/theme.h"
#include "../../gui/desktop.h"
#include "../../gui/window.h"
#include "../../kernel/drivers/graphics/graphics.h"
#include "../../kernel/drivers/mouse/mouse.h"
#include "../../shell/msh.h"


void console_write(const char* text);
void console_clear();
void keyboard_flush();
int keyboard_available();
char keyboard_getchar();
unsigned long timer_ticks();


#define GFX_SCALE 2


void cmd_settings()
{


if (!gfx_available())
{

console_write("Les parametres graphiques necessitent le mode graphique et une souris.\n");

return;

}


int slot = gui_window_claim_slot();

if (slot < 0)
{

return;

}


int win_x = 2, win_y = 2, win_w = 34, win_h = 20;


int maximized = 0;

int saved_x = win_x, saved_y = win_y, saved_w = win_w, saved_h = win_h;


u32 logout_x, logout_y, logout_w, logout_h;

u32 theme_btn_x, theme_btn_y, theme_btn_w, theme_btn_h;

u32 close_bx = 0, close_by = 0, close_bsize = 0;

u32 max_bx = 0, max_by = 0, max_bsize = 0;

u32 min_bx = 0, min_by = 0, min_bsize = 0;


int was_pressed = 0;

int redraw = 1;

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


/*
    Le theme (clair/sombre) peut changer pendant que cette
    fenetre est ouverte -- voir plus bas, clic sur le bouton
    "Theme". "redraw" force alors un nouveau passage complet
    (desktop_render_backdrop() efface tout l'ecran avec le
    fond du NOUVEAU theme et redessine la barre des taches).
    Sert aussi apres un glisser-deposer, un agrandissement/
    restauration, ou une reprise de focus (voir plus haut).
*/

if (redraw)
{


desktop_render_backdrop();

gui_draw_window(
win_x, win_y, win_w, win_h, "Parametres", theme_text_attr(),
&close_bx, &close_by, &close_bsize,
&max_bx, &max_by, &max_bsize, maximized,
&min_bx, &min_by, &min_bsize
);

gui_draw_session_content(win_x, win_y);

gui_draw_button(win_x + 1, win_y + win_h - 6, win_w - 2, 2, "Deconnexion", &logout_x, &logout_y, &logout_w, &logout_h);

gui_draw_button(
win_x + 1, win_y + win_h - 4, win_w - 2, 2,
theme_is_dark() ? "Theme : Sombre" : "Theme : Clair",
&theme_btn_x, &theme_btn_y, &theme_btn_w, &theme_btn_h
);


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


else if (clicked && !drag.active && gui_point_in_rect(logout_x, logout_y, logout_w, logout_h, mx, my))
{

/*
    shell_request_logout() (shell/msh.c) pose un drapeau lu
    par gui_desktop_run() (gui/desktop.c) a chaque tour de
    boucle -- pas par ce thread-ci (chaque fenetre tourne
    desormais dans son propre thread, voir gui/window.h) :
    on se contente donc de poser le drapeau et de fermer
    cette fenetre normalement, le bureau se chargera de la
    deconnexion reelle des qu'il regagnera le focus.
*/

shell_request_logout();

gui_cursor_erase();

gui_window_close(slot);

break;

}


else if (clicked && !drag.active && gui_point_in_rect(theme_btn_x, theme_btn_y, theme_btn_w, theme_btn_h, mx, my))
{

theme_toggle_dark_mode();

redraw = 1;

}


if (keyboard_available())
{

char c = keyboard_getchar();

if (c == '\n')
{

gui_cursor_erase();

gui_window_close(slot);

break;

}

}


unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{
}


}


}
