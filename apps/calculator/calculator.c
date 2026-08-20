#include "calculator.h"

#include "../../gui/gui.h"

#include "../../gui/theme.h"
#include "../../kernel/drivers/graphics/graphics.h"
#include "../../kernel/drivers/mouse/mouse.h"


void console_write(const char* text);
void console_clear();
void keyboard_flush();
int keyboard_available();
char keyboard_getchar();
unsigned long timer_ticks();


static void write_int_buf(int value, char* out)
{

char tmp[12];

int i = 0;

int negative = 0;


if (value < 0)
{

negative = 1;

value = -value;

}


if (value == 0)
{

tmp[i++] = '0';

}


while (value > 0)
{

tmp[i++] = (char)('0' + (value % 10));

value /= 10;

}


int j = 0;

if (negative)
{

out[j++] = '-';

}


while (i > 0)
{

i--;

out[j++] = tmp[i];

}

out[j] = 0;

}


/* Disposition : grille 4x4 de boutons -- partagee entre le dessin et les clics. */

static const char* CALC_LABELS[4][4] = {
{"7", "8", "9", "/"},
{"4", "5", "6", "*"},
{"1", "2", "3", "-"},
{"C", "0", "=", "+"}
};


/*
    Redessine toute la fenetre (cadre, affichage, grille de
    touches) -- appelee au premier affichage ET a chaque fois
    que la fenetre est deplacee par glisser-deposer (voir
    gui_drag_update(), gui/gui.c), puisque win_x/win_y changent
    alors et que tout doit reapparaitre a la nouvelle position.
    Recalcule aussi les zones de clic (bouton de fermeture +
    grille), puisqu'elles dependent elles aussi de win_x/win_y.
*/

static void calc_draw(
int win_x, int win_y, int win_w, int win_h,
const char* display,
u32 btn_x[4][4], u32 btn_y[4][4], u32 btn_w[4][4], u32 btn_h[4][4],
u32* close_bx, u32* close_by, u32* close_bsize
)
{

console_clear();

gui_draw_window(win_x, win_y, win_w, win_h, "Calculatrice", theme_text_attr(), close_bx, close_by, close_bsize);

gui_draw_button(win_x + 1, win_y + 2, win_w - 2, 3, display, (void*)0, (void*)0, (void*)0, (void*)0);


for (int row = 0; row < 4; row++)
{

for (int col = 0; col < 4; col++)
{

int bx = win_x + 1 + col * 5;

int by = win_y + 6 + row * 3;

gui_draw_button(bx, by, 4, 2, CALC_LABELS[row][col], &btn_x[row][col], &btn_y[row][col], &btn_w[row][col], &btn_h[row][col]);

}

}


gui_draw_text(win_x + 1, win_y + win_h - 2, "X pour fermer -- barre de titre pour deplacer", theme_text_attr());

}


void cmd_calc()
{


if (!gfx_available())
{

console_write("La calculatrice necessite le mode graphique et une souris (voir gfxstatus).\n");

return;

}


int win_x = 2, win_y = 2, win_w = 22, win_h = 20;


int current_value = 0;

int pending_value = 0;

char pending_op = 0;

int just_computed = 0;


char display[16];

write_int_buf(current_value, display);


u32 btn_x[4][4], btn_y[4][4], btn_w[4][4], btn_h[4][4];

u32 close_bx, close_by, close_bsize;


calc_draw(win_x, win_y, win_w, win_h, display, btn_x, btn_y, btn_w, btn_h, &close_bx, &close_by, &close_bsize);


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


/*
    Glisser-deposer par la barre de titre (voir gui.h) : a
    tester avant les clics sur les touches, sinon un
    glisser commencant sur la barre de titre pourrait aussi
    etre lu comme un clic sur une touche situee a la meme
    position ecran apres deplacement.
*/

if (gui_drag_update(&drag, &win_x, &win_y, win_w, win_h, mx, my, now_pressed, close_bx, close_by, close_bsize))
{

calc_draw(win_x, win_y, win_w, win_h, display, btn_x, btn_y, btn_w, btn_h, &close_bx, &close_by, &close_bsize);

gui_cursor_reset();

}


if (clicked && gui_point_in_rect(close_bx, close_by, close_bsize, close_bsize, mx, my))
{

break;

}


if (clicked && !drag.active)
{


int hit_row = -1, hit_col = -1;

for (int row = 0; row < 4 && hit_row < 0; row++)
{

for (int col = 0; col < 4; col++)
{

if (gui_point_in_rect(btn_x[row][col], btn_y[row][col], btn_w[row][col], btn_h[row][col], mx, my))
{

hit_row = row;

hit_col = col;

break;

}

}

}


if (hit_row >= 0)
{

const char* label = CALC_LABELS[hit_row][hit_col];


if (label[0] >= '0' && label[0] <= '9')
{

if (just_computed)
{

current_value = 0;

just_computed = 0;

}

current_value = current_value * 10 + (label[0] - '0');

}
else if (label[0] == 'C')
{

current_value = 0;

pending_value = 0;

pending_op = 0;

just_computed = 0;

}
else if (label[0] == '=')
{

if (pending_op != 0)
{

if (pending_op == '+') { current_value = pending_value + current_value; }
else if (pending_op == '-') { current_value = pending_value - current_value; }
else if (pending_op == '*') { current_value = pending_value * current_value; }
else if (pending_op == '/') { current_value = (current_value != 0) ? pending_value / current_value : 0; }

pending_op = 0;

just_computed = 1;

}

}
else
{

/* Operateur : + - * / */

pending_value = current_value;

pending_op = label[0];

current_value = 0;

}


write_int_buf(current_value, display);

gui_draw_button(win_x + 1, win_y + 2, win_w - 2, 3, display, (void*)0, (void*)0, (void*)0, (void*)0);

}

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
