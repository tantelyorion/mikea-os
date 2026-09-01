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


static void installer_draw(int win_x, int win_y, int win_w, int win_h, int mode, install_result result, u32 failed_sector,
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

gui_draw_text(win_x + 1, win_y + 4, "pour installer MikeaOS.", theme_text_attr());

gui_draw_button(win_x + 1, win_y + win_h - 4, win_w - 2, 2, "Fermer", action_x, action_y, action_w, action_h);

}

else if (mode == INST_MODE_CONFIRM)
{

gui_draw_text(win_x + 1, win_y + 2, "ATTENTION : ceci va EFFACER le", theme_text_attr());

gui_draw_text(win_x + 1, win_y + 3, "disque de donnees actuel et le", theme_text_attr());

gui_draw_text(win_x + 1, win_y + 4, "remplacer par une copie demarrable", theme_text_attr());

gui_draw_text(win_x + 1, win_y + 5, "de MikeaOS.", theme_text_attr());

gui_draw_text(win_x + 1, win_y + 7, "Cette action est irreversible.", theme_text_attr());


gui_draw_button(win_x + 1, win_y + win_h - 4, (win_w - 3) / 2, 2, "Installer", action_x, action_y, action_w, action_h);

gui_draw_button(win_x + 2 + (win_w - 3) / 2, win_y + win_h - 4, (win_w - 3) / 2, 2, "Annuler", cancel_x, cancel_y, cancel_w, cancel_h);

}

else if (mode == INST_MODE_RUNNING)
{

gui_draw_text(win_x + 1, win_y + 2, "Installation en cours...", theme_text_attr());


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

case INSTALL_ERROR_READ: msg = "Echec : erreur de lecture (disque source)."; break;

case INSTALL_ERROR_WRITE: msg = "Echec : erreur d'ecriture (disque cible)."; break;

default: msg = "Echec : verification (donnees differentes)."; break;

}


gui_draw_text(win_x + 1, win_y + 2, msg, theme_text_attr());


if (result == INSTALL_OK)
{

gui_draw_text(win_x + 1, win_y + 4, "Inversez l'ordre des deux disques", theme_text_attr());

gui_draw_text(win_x + 1, win_y + 5, "dans QEMU pour demarrer sur la", theme_text_attr());

gui_draw_text(win_x + 1, win_y + 6, "copie installee.", theme_text_attr());

}
else
{

/*
    Correctif (diagnostic d'echec invisible en mode
    graphique -- voir boot/installer/installer.h,
    "failed_sector_out") : indique EXACTEMENT quel secteur
    a pose probleme, directement dans la fenetre --
    jusqu'ici, seul un console_write() invisible en mode
    graphique donnait ce detail.
*/

char sector_line[40];

const char* prefix = "Secteur concerne : ";

int i = 0;

while (prefix[i] && i < 39) { sector_line[i] = prefix[i]; i++; }


u32 v = failed_sector;

char tmp[12];

int t = 0;

if (v == 0) { tmp[t++] = '0'; }

while (v > 0) { tmp[t++] = (char)('0' + (v % 10)); v /= 10; }

while (t > 0 && i < 39) { t--; sector_line[i++] = tmp[t]; }

sector_line[i] = 0;


gui_draw_text(win_x + 1, win_y + 4, sector_line, theme_text_attr());

gui_draw_text(win_x + 1, win_y + 6, "Reessayez ; si l'echec persiste,", theme_text_attr());

gui_draw_text(win_x + 1, win_y + 7, "le disque cible pose probleme.", theme_text_attr());

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


int win_x = 1, win_y = 4, win_w = 38, win_h = 14;


user* current = user_get_current();

int mode = (current != (void*)0 && current->id == 1) ? INST_MODE_CONFIRM : INST_MODE_DENIED;

install_result result = INSTALL_OK;

u32 failed_sector = 0;


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

installer_draw(win_x, win_y, win_w, win_h, mode, result, failed_sector, &close_bx, &close_by, &close_bsize, &action_x, &action_y, &action_w, &action_h, &cancel_x, &cancel_y, &cancel_w, &cancel_h);

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


/*
    Correctif (perception de "plantage" au clic sur
    "Installer") : sans cet appel explicite ici,
    l'ecran restait fige sur l'ecran de confirmation
    pendant TOUTE la duree de l'installation (1 a 2
    minutes, voir les delais de securite ATA -- boot/
    installer/installer.c, ata_delay_ticks()) avant
    d'afficher quoi que ce soit -- "redraw = 1" plus
    bas ne suffit pas : le prochain passage par "if
    (redraw)" (haut de la boucle) n'a jamais lieu avant
    la fin de installer_run() ci-dessous, puisque cet
    appel est BLOQUANT et se produit sur cette MEME
    iteration, juste apres. Sans retour visuel pendant
    1 a 2 minutes, l'ecran fige donnait exactement
    l'impression d'un systeme plante.
*/

installer_draw(win_x, win_y, win_w, win_h, mode, result, failed_sector, &close_bx, &close_by, &close_bsize, &action_x, &action_y, &action_w, &action_h, &cancel_x, &cancel_y, &cancel_w, &cancel_h);


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

result = installer_run(installer_progress_cb, &failed_sector);

mode = INST_MODE_DONE;

redraw = 1;

}


unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{
}


}


}
