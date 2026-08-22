#include "calculator.h"

#include "../../gui/gui.h"

#include "../../gui/theme.h"
#include "../../gui/desktop.h"
#include "../../gui/window.h"
#include "../../kernel/drivers/graphics/graphics.h"
#include "../../kernel/drivers/mouse/mouse.h"


void console_write(const char* text);
void console_clear();
void keyboard_flush();
int keyboard_available();
char keyboard_getchar();
unsigned long timer_ticks();


#define GFX_SCALE 2


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
    touches) -- appelee au premier affichage, apres un
    deplacement/redimensionnement, ET a chaque fois que cette
    fenetre regagne le focus (voir gui/window.h : sa reduction/
    restauration n'efface rien, mais l'ecran affiche entre-temps
    autre chose, donc tout redessiner est necessaire des le
    retour).

    Le bureau (fond + barre des taches, voir
    desktop_render_backdrop()) est redessine derriere la
    fenetre a chaque fois plutot qu'un simple console_clear().
*/

static void calc_draw(
int win_x, int win_y, int win_w, int win_h, int maximized,
const char* display,
u32 btn_x[4][4], u32 btn_y[4][4], u32 btn_w[4][4], u32 btn_h[4][4],
u32* close_bx, u32* close_by, u32* close_bsize,
u32* max_bx, u32* max_by, u32* max_bsize,
u32* min_bx, u32* min_by, u32* min_bsize
)
{

desktop_render_backdrop();

gui_draw_window(
win_x, win_y, win_w, win_h, "Calculatrice", theme_text_attr(),
close_bx, close_by, close_bsize,
max_bx, max_by, max_bsize, maximized,
min_bx, min_by, min_bsize
);

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


}


void cmd_calc()
{


if (!gfx_available())
{

console_write("La calculatrice necessite le mode graphique et une souris (voir gfxstatus).\n");

return;

}


/*
    Le bureau (gui/desktop.c) a deja cree ce thread et lui a
    deja donne le focus (voir gui_window_open()) : on recupere
    juste notre numero de fenetre pour pouvoir ceder/reprendre
    le focus nous-memes ensuite (reduire, fermer).
*/

int slot = gui_window_claim_slot();

if (slot < 0)
{

return;

}


int win_x = 2, win_y = 2, win_w = 22, win_h = 20;


int maximized = 0;

int saved_x = win_x, saved_y = win_y, saved_w = win_w, saved_h = win_h;


int current_value = 0;

int pending_value = 0;

char pending_op = 0;

int just_computed = 0;


char display[16];

write_int_buf(current_value, display);


u32 btn_x[4][4], btn_y[4][4], btn_w[4][4], btn_h[4][4];

u32 close_bx = 0, close_by = 0, close_bsize = 0;

u32 max_bx = 0, max_by = 0, max_bsize = 0;

u32 min_bx = 0, min_by = 0, min_bsize = 0;


int redraw = 1;

int was_pressed = 0;

gui_drag drag;

drag.active = 0;


while (1)
{


/*
    Jeton de focus (voir gui/window.h) : cette fenetre peut
    exister sans etre visible (reduite, ou une autre fenetre
    a le focus) -- dans ce cas, ne rien dessiner ni lire
    la souris/le clavier, juste attendre. "redraw = 1" est
    pose des maintenant pour forcer un redessin complet des
    que le focus revient.
*/

if (!gui_window_has_focus(slot))
{

redraw = 1;

gui_window_idle();

continue;

}


if (redraw)
{

calc_draw(win_x, win_y, win_w, win_h, maximized, display, btn_x, btn_y, btn_w, btn_h, &close_bx, &close_by, &close_bsize, &max_bx, &max_by, &max_bsize, &min_bx, &min_by, &min_bsize);

/*
    Meme precaution que gui/desktop.c a la reprise du
    focus : capturer l'etat REEL du bouton plutot que de
    supposer qu'il est relache, pour ne pas confondre un
    clic deja en cours (ex. sur le bouton de restauration
    depuis la barre des taches) avec un nouveau clic dans
    cette fenetre.
*/

was_pressed = mouse_left_pressed();

redraw = 0;

}


s32 mx = mouse_get_x();

s32 my = mouse_get_y();

gui_draw_cursor(mx, my);


int now_pressed = mouse_left_pressed();

int clicked = (now_pressed && !was_pressed);

was_pressed = now_pressed;


/*
    Glisser-deposer par la barre de titre (voir gui.h) : a
    tester avant les clics sur les touches. Desactive en
    plein ecran (comme sur un bureau habituel).
*/

if (!maximized && gui_drag_update(&drag, &win_x, &win_y, win_w, win_h, mx, my, now_pressed, close_bx, close_by, close_bsize))
{

calc_draw(win_x, win_y, win_w, win_h, maximized, display, btn_x, btn_y, btn_w, btn_h, &close_bx, &close_by, &close_bsize, &max_bx, &max_by, &max_bsize, &min_bx, &min_by, &min_bsize);

gui_cursor_reset();

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


calc_draw(win_x, win_y, win_w, win_h, maximized, display, btn_x, btn_y, btn_w, btn_h, &close_bx, &close_by, &close_bsize, &max_bx, &max_by, &max_bsize, &min_bx, &min_by, &min_bsize);

gui_cursor_reset();

was_pressed = 0;

}


else if (clicked && !drag.active)
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
