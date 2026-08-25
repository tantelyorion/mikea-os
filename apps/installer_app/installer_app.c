#include "installer_app.h"

#include "../../gui/gui.h"

#include "../../gui/theme.h"
#include "../../gui/desktop.h"
#include "../../gui/window.h"
#include "../../kernel/drivers/graphics/graphics.h"
#include "../../kernel/drivers/mouse/mouse.h"
#include "../../boot/installer/installer.h"
#include "../../security/user.h"


void console_write(const char* text);

void keyboard_flush();

int keyboard_available();

char keyboard_getchar();

unsigned long timer_ticks();


#define GFX_SCALE 2

#define INST_MODE_CONFIRM 0

#define INST_MODE_RUNNING 1

#define INST_MODE_DONE    2

#define INST_MODE_DENIED  3


/*
    Etat de la barre de progression, en variables de module
    plutot qu'en parametre : installer_progress_cb() doit
    correspondre EXACTEMENT a la signature imposee par
    installer_run() (boot/installer/installer.h), qui ne
    prevoit pas de transmettre de contexte utilisateur.
*/

static u32 g_bar_px, g_bar_py, g_bar_pw, g_bar_ph;


static void installer_progress_cb(u32 sector, u32 total)
{

/*
    Ne redessine que toutes les 32 secteurs (pas a chaque
    secteur, 2048 fois par installation) : suffisant pour une
    barre de progression fluide a l'oeil, sans ralentir la
    copie elle-meme avec un redessin trop frequent.
*/

if (sector % 32 != 0 && sector + 1 != total)
{

return;

}


u32 percent = (total > 0) ? (sector * 100) / total : 0;


gfx_draw_rect(g_bar_px, g_bar_py, g_bar_pw, g_bar_ph, theme_border());


u32 fill_w = (g_bar_pw > 2) ? ((g_bar_pw - 2) * percent) / 100 : 0;

gfx_fill_rect(g_bar_px + 1, g_bar_py + 1, g_bar_pw - 2, g_bar_ph - 2, theme_panel());

if (fill_w > 0)
{

gfx_fill_rect(g_bar_px + 1, g_bar_py + 1, fill_w, g_bar_ph - 2, theme_text());

}


char percent_text[8];

int i = 0;

if (percent >= 100)
{

percent_text[i++] = '1'; percent_text[i++] = '0'; percent_text[i++] = '0';

}
else
{

if (percent >= 10) { percent_text[i++] = (char)('0' + (percent / 10)); }

percent_text[i++] = (char)('0' + (percent % 10));

}

percent_text[i++] = '%';

percent_text[i] = 0;


gfx_draw_text(g_bar_px + g_bar_pw + 8, g_bar_py, percent_text, theme_text(), GFX_SCALE);

}


static void installer_draw(int win_x, int win_y, int win_w, int win_h, int mode, install_result result,
u32* close_bx, u32* close_by, u32* close_bsize,
u32* action_x, u32* action_y, u32* action_w, u32* action_h,
u32* cancel_x, u32* cancel_y, u32* cancel_w, u32* cancel_h)
{

desktop_render_backdrop();

gui_draw_window(win_x, win_y, win_w, win_h, "Installateur", theme_text_attr(), close_bx, close_by, close_bsize, (void*)0, (void*)0, (void*)0, 0, (void*)0, (void*)0, (void*)0);


if (mode == INST_MODE_DENIED)
{

gui_draw_text(win_x + 1, win_y + 2, "Reserve a root.", theme_text_attr());

gui_draw_text(win_x + 1, win_y + 3, "Connectez-vous en administrateur", theme_text_attr());

gui_draw_text(win_x + 1, win_y + 4, "pour installer MikeaOS sur le disque.", theme_text_attr());

gui_draw_button(win_x + 1, win_y + win_h - 4, win_w - 2, 2, "Fermer", action_x, action_y, action_w, action_h);

}

else if (mode == INST_MODE_CONFIRM)
{

gui_draw_text(win_x + 1, win_y + 2, "ATTENTION : ceci va EFFACER le disque", theme_text_attr());

gui_draw_text(win_x + 1, win_y + 3, "de donnees actuel et le remplacer par", theme_text_attr());

gui_draw_text(win_x + 1, win_y + 4, "une copie demarrable de MikeaOS.", theme_text_attr());

gui_draw_text(win_x + 1, win_y + 6, "Cette action est irreversible.", theme_text_attr());


gui_draw_button(win_x + 1, win_y + win_h - 4, (win_w - 3) / 2, 2, "Installer", action_x, action_y, action_w, action_h);

gui_draw_button(win_x + 2 + (win_w - 3) / 2, win_y + win_h - 4, (win_w - 3) / 2, 2, "Annuler", cancel_x, cancel_y, cancel_w, cancel_h);

}

else if (mode == INST_MODE_RUNNING)
{

gui_draw_text(win_x + 1, win_y + 2, "Installation en cours, veuillez patienter...", theme_text_attr());


g_bar_px = (u32)(win_x + 1) * 8 * GFX_SCALE;

g_bar_py = (u32)(win_y + 4) * 8 * GFX_SCALE;

g_bar_pw = (u32)(win_w - 12) * 8 * GFX_SCALE;

g_bar_ph = 8 * GFX_SCALE;


installer_progress_cb(0, INSTALLER_SECTOR_COUNT);

}

else /* INST_MODE_DONE */
{

const char* msg;

switch (result)
{

case INSTALL_OK: msg = "Installation reussie."; break;

case INSTALL_ERROR_READ: msg = "Echec : erreur de lecture du disque source."; break;

case INSTALL_ERROR_WRITE: msg = "Echec : erreur d'ecriture sur le disque cible."; break;

default: msg = "Echec : la verification a trouve des donnees differentes."; break;

}


gui_draw_text(win_x + 1, win_y + 2, msg, theme_text_attr());


if (result == INSTALL_OK)
{

gui_draw_text(win_x + 1, win_y + 4, "Inversez l'ordre des deux disques dans", theme_text_attr());

gui_draw_text(win_x + 1, win_y + 5, "QEMU pour demarrer sur la copie installee.", theme_text_attr());

}


gui_draw_button(win_x + 1, win_y + win_h - 4, win_w - 2, 2, "Fermer", action_x, action_y, action_w, action_h);

}

}


void cmd_installer_app()
{


if (!gfx_available())
{

console_write("L'installateur graphique necessite le mode graphique et une souris.\n");

return;

}


int slot = gui_window_claim_slot();

if (slot < 0)
{

return;

}


int win_x = 4, win_y = 4, win_w = 40, win_h = 12;


user* current = user_get_current();

int mode = (current != (void*)0 && current->id == 1) ? INST_MODE_CONFIRM : INST_MODE_DENIED;

install_result result = INSTALL_OK;


u32 close_bx = 0, close_by = 0, close_bsize = 0;

u32 action_x = 0, action_y = 0, action_w = 0, action_h = 0;

u32 cancel_x = 0, cancel_y = 0, cancel_w = 0, cancel_h = 0;


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

installer_draw(win_x, win_y, win_w, win_h, mode, result, &close_bx, &close_by, &close_bsize, &action_x, &action_y, &action_w, &action_h, &cancel_x, &cancel_y, &cancel_w, &cancel_h);

was_pressed = mouse_left_pressed();

redraw = 0;

}


s32 mx = mouse_get_x();

s32 my = mouse_get_y();

gui_draw_cursor(mx, my);


int now_pressed = mouse_left_pressed();

int clicked = (now_pressed && !was_pressed);

was_pressed = now_pressed;


if (mode != INST_MODE_RUNNING && clicked)
{

u32 rows_now = gfx_height() / (8 * GFX_SCALE);

u32 taskbar_top_px = (rows_now - 2) * 8 * GFX_SCALE;

if ((u32)my >= taskbar_top_px)
{

gui_cursor_erase();

gui_window_yield_click(slot);

continue;

}

}


if (mode != INST_MODE_RUNNING && !drag.active)
{

gui_drag_update(&drag, &win_x, &win_y, win_w, win_h, mx, my, now_pressed, close_bx, close_by, close_bsize);

}


if (mode != INST_MODE_RUNNING && clicked && gui_point_in_rect(close_bx, close_by, close_bsize, close_bsize, mx, my))
{

gui_cursor_erase();

gui_window_close(slot);

break;

}


else if (clicked && gui_point_in_rect(action_x, action_y, action_w, action_h, mx, my))
{

if (mode == INST_MODE_CONFIRM)
{

mode = INST_MODE_RUNNING;

redraw = 1;

}
else
{

/* "Fermer" (DENIED ou DONE). */

gui_cursor_erase();

gui_window_close(slot);

break;

}

}


else if (mode == INST_MODE_CONFIRM && clicked && gui_point_in_rect(cancel_x, cancel_y, cancel_w, cancel_h, mx, my))
{

gui_cursor_erase();

gui_window_close(slot);

break;

}


if (mode == INST_MODE_RUNNING)
{

/*
    Redessine "en cours" une seule fois (ci-dessus, via
    "redraw"), puis lance l'installation -- bloquante le
    temps de l'operation (quelques secondes), avec une
    barre de progression mise a jour en direct par
    installer_progress_cb() a chaque tranche de secteurs
    copies. Voir boot/installer/installer.h.
*/

result = installer_run(installer_progress_cb);

mode = INST_MODE_DONE;

redraw = 1;

}


unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{
}


}


}
