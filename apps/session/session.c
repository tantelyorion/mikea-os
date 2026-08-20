#include "session.h"

#include "../../gui/gui.h"

#include "../../gui/theme.h"
#include "../../kernel/drivers/graphics/graphics.h"
#include "../../kernel/drivers/mouse/mouse.h"
#include "../../security/user.h"
#include "../../security/permission.h"


void console_write(const char* text);
void console_clear();
void keyboard_flush();
int keyboard_available();
char keyboard_getchar();
void input_readline(char* buffer, unsigned int max_len);
unsigned long timer_ticks();


void gui_draw_session_content(int win_x, int win_y)
{


user* current = user_get_current();

if(current == 0)
{

gui_draw_text(win_x + 3, win_y + 4, "Aucun utilisateur connecte.", theme_text_attr());

return;

}


/*
    "Utilisateur : " + nom, construit caractere par
    caractere (pas de mk_strcat, voir la note a ce sujet
    dans shell/msh.c).
*/

char line[48];

const char* label = "Utilisateur : ";

int i = 0;

while(label[i] && i < 47)
{

line[i] = label[i];

i++;

}

int j = 0;

while(current->username[j] && i < 47)
{

line[i] = current->username[j];

i++;

j++;

}

line[i] = 0;

gui_draw_text(win_x + 3, win_y + 3, line, theme_text_attr());


if(current->id == 1)
{

gui_draw_text(win_x + 3, win_y + 4, "Role : administrateur (root)", theme_text_attr());

}
else
{

gui_draw_text(win_x + 3, win_y + 4, "Role : utilisateur standard", theme_text_attr());

}


gui_draw_text(win_x + 3, win_y + 6, "Permissions :", theme_text_attr());

gui_draw_text(
win_x + 5, win_y + 7,
check_permission((int)current->id, PERMISSION_READ)
? "Lecture    : oui" : "Lecture    : non",
theme_text_attr()
);

gui_draw_text(
win_x + 5, win_y + 8,
check_permission((int)current->id, PERMISSION_WRITE)
? "Ecriture   : oui" : "Ecriture   : non",
theme_text_attr()
);

gui_draw_text(
win_x + 5, win_y + 9,
check_permission((int)current->id, PERMISSION_EXEC)
? "Execution  : oui" : "Execution  : non",
theme_text_attr()
);

}


void cmd_gui()
{


if (!gfx_available())
{

console_clear();

gui_draw_window(2, 2, 36, 16, "Mikea OS - Session", theme_text_attr(), (void*)0, (void*)0, (void*)0);

gui_draw_session_content(2, 2);

gui_draw_text(5, 14, "Appuyez sur Entree pour revenir au shell...", theme_text_attr());


char dummy[8];

keyboard_flush();

input_readline(dummy, sizeof(dummy));


console_clear();

return;

}


u32 close_x = 0;

u32 close_y = 0;

u32 close_size = 0;


int win_x = 2, win_y = 2, win_w = 36, win_h = 16;


console_clear();

gui_draw_window(win_x, win_y, win_w, win_h, "Mikea OS - Session", theme_text_attr(), &close_x, &close_y, &close_size);

gui_draw_session_content(win_x, win_y);

gui_draw_text(win_x + 3, win_y + win_h - 2, "X ou Entree pour fermer -- barre de titre pour deplacer", theme_text_attr());


keyboard_flush();


int was_pressed = 0;

gui_drag drag;

drag.active = 0;


while (1)
{


s32 mx = mouse_get_x();

s32 my = mouse_get_y();

gui_draw_cursor(mx, my);


int now_pressed = mouse_left_pressed();

int clicked = (now_pressed && !was_pressed);

was_pressed = now_pressed;


if (gui_drag_update(&drag, &win_x, &win_y, win_w, win_h, mx, my, now_pressed, close_x, close_y, close_size))
{

console_clear();

gui_draw_window(win_x, win_y, win_w, win_h, "Mikea OS - Session", theme_text_attr(), &close_x, &close_y, &close_size);

gui_draw_session_content(win_x, win_y);

gui_draw_text(win_x + 3, win_y + win_h - 2, "X ou Entree pour fermer -- barre de titre pour deplacer", theme_text_attr());

gui_cursor_reset();

}


if (clicked && gui_point_in_button(close_x, close_y, close_size, mx, my))
{

break;

}


if (keyboard_available())
{

char c = keyboard_getchar();

if (c == '\n')
{

break;

}

}


/*
    Cadence de la boucle (~10ms par image a 100Hz) : evite de
    consommer le CPU en boucle a pleine vitesse pour un gain
    visuel nul.
*/

unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{
}


}


gui_cursor_erase();

console_clear();


}
