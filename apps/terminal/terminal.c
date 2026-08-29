#include "terminal.h"

#include "../../gui/gui.h"

#include "../../gui/theme.h"
#include "../../gui/desktop.h"
#include "../../gui/window.h"
#include "../../kernel/drivers/graphics/graphics.h"
#include "../../kernel/drivers/mouse/mouse.h"
#include "../../kernel/console/console.h"
#include "../../libc/string.h"


void console_write(const char* text);

void keyboard_flush();

int keyboard_available();

char keyboard_getchar();

unsigned long timer_ticks();

void execute_command(char* command);

int mk_strcmp(const char* a, const char* b);


#define GFX_SCALE 2

#define TERM_MAX_LINES 200

#define TERM_LINE_WIDTH 100

#define TERM_VISIBLE_LINES 13

#define TERM_INPUT_MAX 120

#define TERM_OUTPUT_CAPTURE 2048


typedef struct
{

char text[TERM_LINE_WIDTH];

} term_line;


static void term_push_line(term_line* lines, int* count, const char* text)
{

if (*count >= TERM_MAX_LINES)
{

for (int i = 1; i < TERM_MAX_LINES; i++)
{

lines[i - 1] = lines[i];

}

*count = TERM_MAX_LINES - 1;

}


int i = 0;

while (text[i] != 0 && i < TERM_LINE_WIDTH - 1)
{

lines[*count].text[i] = text[i];

i++;

}

lines[*count].text[i] = 0;

(*count)++;

}


static void term_push_block(term_line* lines, int* count, const char* block)
{

char buf[TERM_LINE_WIDTH];

int bi = 0;

int i = 0;


while (block[i] != 0)
{

if (block[i] == '\n')
{

buf[bi] = 0;

term_push_line(lines, count, buf);

bi = 0;

}
else if (bi < TERM_LINE_WIDTH - 1)
{

buf[bi++] = block[i];

}

i++;

}


if (bi > 0)
{

buf[bi] = 0;

term_push_line(lines, count, buf);

}

}


static void term_draw(
int win_x, int win_y, int win_w, int win_h, int maximized,
term_line* lines, int line_count, const char* input_buf,
u32* close_bx, u32* close_by, u32* close_bsize,
u32* max_bx, u32* max_by, u32* max_bsize,
u32* min_bx, u32* min_by, u32* min_bsize
)
{

desktop_render_backdrop();

gui_draw_window(
win_x, win_y, win_w, win_h, "Terminal", theme_text_attr(),
close_bx, close_by, close_bsize,
max_bx, max_by, max_bsize, maximized,
min_bx, min_by, min_bsize
);


u32 px = (u32)win_x * 8 * GFX_SCALE;

u32 py = (u32)win_y * 8 * GFX_SCALE;

u32 pw = (u32)win_w * 8 * GFX_SCALE;

u32 ph = (u32)win_h * 8 * GFX_SCALE;

u32 titlebar_h = 8 * GFX_SCALE + 4;


gfx_fill_rect(px + 2, py + titlebar_h, pw - 4, ph - titlebar_h - 4, 0x000000);


gfx_color term_fg = 0xE0E0E0;


int visible = (line_count < TERM_VISIBLE_LINES) ? line_count : TERM_VISIBLE_LINES;

int start = line_count - visible;


for (int i = 0; i < visible; i++)
{

gfx_draw_text(px + 8, py + titlebar_h + 4 + (u32)i * 8 * GFX_SCALE, lines[start + i].text, term_fg, GFX_SCALE);

}


char prompt_display[TERM_INPUT_MAX + 4];

prompt_display[0] = '$';

prompt_display[1] = ' ';

int pi = 2, ii = 0;

while (input_buf[ii] != 0 && pi < TERM_INPUT_MAX + 2)
{

prompt_display[pi++] = input_buf[ii++];

}

prompt_display[pi] = 0;


gfx_draw_text(px + 8, py + titlebar_h + 4 + (u32)TERM_VISIBLE_LINES * 8 * GFX_SCALE, prompt_display, term_fg, GFX_SCALE);

}


void cmd_terminal_app()
{


if (!gfx_available())
{

console_write("Le Terminal graphique necessite le mode graphique et une souris.\n");

return;

}


int slot = gui_window_claim_slot();

if (slot < 0)
{

return;

}


int win_x = 2, win_y = 2, win_w = 46, win_h = 20;

int maximized = 0;

int saved_x = win_x, saved_y = win_y, saved_w = win_w, saved_h = win_h;


static term_line lines[TERM_MAX_LINES];

int line_count = 0;

term_push_line(lines, &line_count, "Terminal Mikea OS");

term_push_line(lines, &line_count, "Tapez 'help' pour la liste des commandes.");

term_push_line(lines, &line_count, "");


char input_buf[TERM_INPUT_MAX];

input_buf[0] = 0;


u32 close_bx = 0, close_by = 0, close_bsize = 0;

u32 max_bx = 0, max_by = 0, max_bsize = 0;

u32 min_bx = 0, min_by = 0, min_bsize = 0;


int redraw = 1;

int was_pressed = 0;

gui_drag drag;

drag.active = 0;


keyboard_flush();


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

term_draw(win_x, win_y, win_w, win_h, maximized, lines, line_count, input_buf, &close_bx, &close_by, &close_bsize, &max_bx, &max_by, &max_bsize, &min_bx, &min_by, &min_bsize);

was_pressed = mouse_left_pressed();

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

/*
    Correctif (Dock macOS, voir gui/desktop.h) : la limite
    utilisee ici venait auparavant d'un calcul local suppose
    ("les 2 dernieres lignes de texte"), perime depuis le
    passage a un Dock flottant de hauteur fixe -- voir
    desktop_dock_top_px().
*/

if ((u32)my >= desktop_dock_top_px())
{

gui_cursor_erase();

gui_window_yield_click(slot);

continue;

}

}


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


if (keyboard_available())
{

char c = keyboard_getchar();


if (c == '\n')
{


char echoed[TERM_INPUT_MAX + 4];

echoed[0] = '$';

echoed[1] = ' ';

int ei = 2, si = 0;

while (input_buf[si] != 0 && ei < TERM_INPUT_MAX + 2)
{

echoed[ei++] = input_buf[si++];

}

echoed[ei] = 0;

term_push_line(lines, &line_count, echoed);


if (mk_strcmp(input_buf, "exit") == 0)
{

gui_cursor_erase();

gui_window_close(slot);

break;

}


static char output[TERM_OUTPUT_CAPTURE];

console_redirect_start(output, sizeof(output));

execute_command(input_buf);

console_redirect_stop();

term_push_block(lines, &line_count, output);


if (mk_strcmp(input_buf, "logout") == 0)
{

gui_cursor_erase();

gui_window_close(slot);

break;

}


input_buf[0] = 0;

redraw = 1;

}
else if (c == '\b')
{

u32 len = mk_strlen(input_buf);

if (len > 0)
{

input_buf[len - 1] = 0;

}

redraw = 1;

}
else if (c != 0)
{

u32 len = mk_strlen(input_buf);

if (len < TERM_INPUT_MAX - 1)
{

input_buf[len] = c;

input_buf[len + 1] = 0;

}

redraw = 1;

}

}


unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{
}


}


}
