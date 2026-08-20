#include "login_screen.h"

#include "gui.h"

#include "theme.h"

#include "../kernel/drivers/graphics/graphics.h"

#include "../kernel/drivers/mouse/mouse.h"

#include "../libc/string.h"


void console_clear();

void keyboard_flush();

int keyboard_available();

char keyboard_getchar();

unsigned long timer_ticks();


#define GFX_SCALE 2

#define LOGIN_FIELD_MAX 30


static u32 login_cols()
{

return gfx_width() / (8 * GFX_SCALE);

}


static u32 login_rows()
{

return gfx_height() / (8 * GFX_SCALE);

}


typedef struct
{

u32 x, y, w, h;

} login_hitbox;


user* gui_login_screen()
{


if (!gfx_available())
{

/*
    Garde-fou (voir gui/desktop.c pour la meme logique) :
    security/login.c ne devrait appeler cette fonction que
    lorsque gfx_available() est deja vrai.
*/

return (void*)0;

}


char username[LOGIN_FIELD_MAX + 1];

username[0] = 0;

char password[LOGIN_FIELD_MAX + 1];

password[0] = 0;


int active_field = 0; /* 0 = utilisateur, 1 = mot de passe */

const char* error_message = (void*)0;


login_hitbox user_box, pass_box, submit_box;


u32 cols = login_cols();

u32 rows = login_rows();


int panel_w = 34;

if (panel_w > (int)cols - 4)
{

panel_w = (int)cols - 4;

}

int panel_h = 15;

if (panel_h > (int)rows - 4)
{

panel_h = (int)rows - 4;

}

int panel_x = ((int)cols - panel_w) / 2;

int panel_y = ((int)rows - panel_h) / 2;


int redraw = 1;

int submit_requested = 0;


gui_cursor_reset();

keyboard_flush();


int was_pressed = 0;


while (1)
{


if (redraw)
{


console_clear();


u32 ppx = (u32)panel_x * 8 * GFX_SCALE;

u32 ppy = (u32)panel_y * 8 * GFX_SCALE;

u32 ppw = (u32)panel_w * 8 * GFX_SCALE;

u32 pph = (u32)panel_h * 8 * GFX_SCALE;


gfx_fill_rect_blend(ppx + 4, ppy + 4, ppw, pph, theme_shadow(), 22);

gfx_fill_rect_blend(ppx, ppy, ppw, pph, theme_panel(), theme_panel_opacity());

gfx_draw_rect(ppx, ppy, ppw, pph, theme_border());


gfx_draw_text(ppx + 16, ppy + 16, "Mikea OS", theme_text(), 2);

gfx_draw_text(ppx + 16, ppy + 44, "Connexion", theme_text(), 1);


gui_draw_text(panel_x + 2, panel_y + 4, "Utilisateur", theme_text_attr());

gui_draw_field(
panel_x + 2, panel_y + 5, panel_w - 4, 2,
username, 0, active_field == 0,
&user_box.x, &user_box.y, &user_box.w, &user_box.h
);


gui_draw_text(panel_x + 2, panel_y + 7, "Mot de passe", theme_text_attr());

gui_draw_field(
panel_x + 2, panel_y + 8, panel_w - 4, 2,
password, 1, active_field == 1,
&pass_box.x, &pass_box.y, &pass_box.w, &pass_box.h
);


gui_draw_button(
panel_x + 2, panel_y + 10, panel_w - 4, 2, "Se connecter",
&submit_box.x, &submit_box.y, &submit_box.w, &submit_box.h
);


if (error_message != (void*)0)
{

u32 err_px = (u32)(panel_x + 2) * 8 * GFX_SCALE;

u32 err_py = (u32)(panel_y + 12) * 8 * GFX_SCALE;

gfx_draw_text(err_px, err_py, error_message, theme_close_bg(), 1);

}


u32 hint_px = (u32)(panel_x + 2) * 8 * GFX_SCALE;

u32 hint_py = (u32)(panel_y + panel_h - 1) * 8 * GFX_SCALE;

gfx_draw_text(hint_px, hint_py, "Par defaut : root / mikea", theme_border(), 1);


redraw = 0;

}


s32 mx = mouse_get_x();

s32 my = mouse_get_y();

gui_draw_cursor(mx, my);


int now_pressed = mouse_left_pressed();

int clicked = (now_pressed && !was_pressed);

was_pressed = now_pressed;


if (clicked)
{

if (gui_point_in_rect(user_box.x, user_box.y, user_box.w, user_box.h, mx, my))
{

active_field = 0;

redraw = 1;

}

else if (gui_point_in_rect(pass_box.x, pass_box.y, pass_box.w, pass_box.h, mx, my))
{

active_field = 1;

redraw = 1;

}

else if (gui_point_in_rect(submit_box.x, submit_box.y, submit_box.w, submit_box.h, mx, my))
{

submit_requested = 1;

}

}


if (keyboard_available())
{

char c = keyboard_getchar();

char* field = (active_field == 0) ? username : password;


if (c == '\n')
{

if (active_field == 0)
{

active_field = 1;

}

else
{

submit_requested = 1;

}

redraw = 1;

}

else if (c == '\b')
{

u32 len = mk_strlen(field);

if (len > 0)
{

field[len - 1] = 0;

}

redraw = 1;

}

else if (c != 0)
{

u32 len = mk_strlen(field);

if (len < LOGIN_FIELD_MAX)
{

field[len] = c;

field[len + 1] = 0;

}

redraw = 1;

}

}


if (submit_requested)
{

submit_requested = 0;


user* logged_in = user_login(username, password);


if (logged_in != (void*)0)
{

gui_cursor_erase();

console_clear();

return logged_in;

}


error_message = "Identifiant ou mot de passe incorrect";

password[0] = 0;

active_field = 1;

keyboard_flush();

redraw = 1;

}


unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{
}


}


}
