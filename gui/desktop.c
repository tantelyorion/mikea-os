#include "desktop.h"

#include "gui.h"

#include "theme.h"

#include "icons.h"

#include "window.h"

#include "../kernel/drivers/graphics/graphics.h"

#include "../kernel/drivers/mouse/mouse.h"

#include "../kernel/drivers/rtc/rtc.h"

#include "../kernel/drivers/power/power.h"

#include "../kernel/drivers/speaker/speaker.h"


/*
    Declarations "externes" plutot qu'un #include des headers
    correspondants : meme convention que le reste des
    applications graphiques (apps/calculator/calculator.c,
    apps/file_manager/file_manager.c...), pour eviter de faire
    dependre gui/ de shell/ et security/ au niveau des headers
    (seul ce .c a besoin de ces quelques fonctions).
*/

void console_write(const char* text);

void console_clear();

void keyboard_flush();

int keyboard_available();

char keyboard_getchar();

void input_readline(char* buffer, unsigned int max_len);

unsigned long timer_ticks();

int mk_strcmp(const char* a, const char* b);

void execute_command(char* command);

int shell_logout_was_requested();

void cmd_files();

void cmd_calc();

void cmd_settings();

void cmd_gui();

void cmd_trash_app();


void console_set_top_margin(int rows);


static void desktop_draw_console_titlebar(const char* title);


#define GFX_SCALE 2


static u32 desktop_cols()
{

return gfx_width() / (8 * GFX_SCALE);

}


static u32 desktop_rows()
{

return gfx_height() / (8 * GFX_SCALE);

}


/*
    Convertit une valeur en decimal dans "out", avec un
    minimum de "min_digits" chiffres (complete par des zeros a
    gauche si besoin -- utile pour "05" plutot que "5" dans
    l'horloge ci-dessous). Pas de sprintf en freestanding :
    meme esprit que write_int_buf() dans
    apps/calculator/calculator.c.
*/

static int write_uint_padded(unsigned long value, int min_digits, char* out)
{

char tmp[20];

int i = 0;


if (value == 0)
{

tmp[i++] = '0';

}


while (value > 0)
{

tmp[i++] = (char)('0' + (value % 10));

value /= 10;

}


while (i < min_digits)
{

tmp[i++] = '0';

}


int j = 0;

while (i > 0)
{

i--;

out[j++] = tmp[i];

}

return j;

}


/*
    "HH:MM" -- horloge murale reelle (kernel/drivers/rtc), voir
    le commentaire de rtc.h pour les limites assumees (pas de
    fuseau horaire).
*/

static void format_walltime(char* out)
{

rtc_time now;

rtc_read(&now);


int i = 0;

i += write_uint_padded(now.hour, 2, out + i);

out[i++] = ':';

i += write_uint_padded(now.minute, 2, out + i);

out[i] = 0;

}


/*
    Terminal integre au bureau : reutilise execute_command()
    (shell/commands.c), donc TOUTES les commandes existantes
    (mpm, mkfs, ps, users...) restent disponibles sans avoir a
    les reimplementer. "exit" (propre a cette fenetre, pas une
    commande shell existante) revient simplement au bureau ;
    "logout" (commande shell existante) est detectee via
    shell_logout_was_requested() pour remonter la demande de
    deconnexion jusqu'a gui_desktop_run().

    Reste volontairement HORS du systeme de fenetres (gui/
    window.c) : il repose sur une lecture ligne par ligne
    bloquante (input_readline()) plutot qu'un sondage image par
    image de la souris/du clavier comme les autres applications
    -- l'adapter au modele "cede le focus des qu'on n'est plus
    actif" exigerait de reecrire sa boucle de saisie de zero.
    Consequence assumee : ouvrir le Terminal bloque le bureau
    (comme avant), le Centre d'applications reste inaccessible
    tant qu'il est ouvert. A defaut d'une vraie fenetre
    cliquable, une barre de titre est tout de meme dessinee
    (voir desktop_draw_console_titlebar()) pour rester coherent
    visuellement avec le reste du bureau.
*/

static int desktop_run_terminal()
{

char command[128];


console_clear();

desktop_draw_console_titlebar("Terminal");

console_set_top_margin(1);

console_write("Terminal Mikea OS\n");

console_write("Tapez 'help' pour la liste des commandes, 'exit' pour revenir au bureau.\n\n");


keyboard_flush();


while (1)
{

console_write("$ ");

input_readline(command, sizeof(command));


if (mk_strcmp(command, "exit") == 0)
{

console_set_top_margin(0);

return 0;

}


execute_command(command);


if (shell_logout_was_requested())
{

console_set_top_margin(0);

return 1;

}


console_write("\n");

}

}


typedef struct
{

u32 x, y, w, h;

} desktop_hitbox;


/*
    Barre de titre minimale pour les applications texte plein
    ecran (Terminal, Installateur) : contrairement aux fenetres
    "normales" (voir gui/gui.c, gui_draw_window()), ces deux-la
    utilisent la console texte (console_write()/input_readline(),
    kernel/console/console.c) plutot qu'un rendu image par image
    avec sondage de la souris -- les integrer au meme systeme de
    fenetres a bouton fermer/reduire/agrandir cliquables
    exigerait de reecrire leur lecture d'entree de zero (voir le
    commentaire de desktop_run_terminal() plus bas). A defaut,
    cette bande de titre les distingue au moins visuellement
    d'une console plein ecran brute, avec le texte qui defile
    EN DESSOUS grace a console_set_top_margin() (voir
    kernel/console/console.h) -- jamais par-dessus.

    Limite assumee : si le texte affiche utilise la commande
    shell "clear" (efface tout, y compris cette bande), la
    barre de titre disparait jusqu'a la prochaine ouverture --
    cas marginal, non traite ici pour rester dans un temps
    raisonnable.
*/

static void desktop_draw_console_titlebar(const char* title)
{

u32 bar_w = gfx_width();

u32 bar_h = 8 * GFX_SCALE + 4;


gfx_fill_rect_blend(0, 0, bar_w, bar_h, theme_titlebar_bg(), 95);

gfx_draw_hline(0, bar_h - 1, bar_w, theme_border());

gfx_draw_text(8 * GFX_SCALE, 2, title, theme_titlebar_text(), GFX_SCALE);

}


/*
    Actions du Centre d'applications. Les cinq premieres sont de
    VRAIES applications, lancees comme des fenetres independantes
    (voir gui/window.c) via "window_entry" -- elles peuvent
    desormais rester ouvertes en arriere-plan, etre reduites, et
    coexister en plusieurs exemplaires. Les cinq dernieres
    restent des actions systeme ponctuelles, executees directement
    par le thread du bureau (bloquantes, mais breves) -- pas de
    sens a les "reduire" ou en avoir plusieurs a la fois.
*/

typedef enum
{

DESKTOP_ACTION_FILES,

DESKTOP_ACTION_CALC,

DESKTOP_ACTION_SETTINGS,

DESKTOP_ACTION_ACCOUNT,

DESKTOP_ACTION_TRASH,

DESKTOP_ACTION_TERMINAL,

DESKTOP_ACTION_LOGOUT,

DESKTOP_ACTION_REBOOT,

DESKTOP_ACTION_SHUTDOWN,

DESKTOP_ACTION_INSTALL

} desktop_action;


#define APP_CENTER_ENTRY_COUNT 10


typedef struct
{

const char* label;

desktop_action action;

/* Non nul pour les applications lancees comme fenetre (voir gui/window.c). */

void (*window_entry)();

} app_center_entry;


static const app_center_entry APP_CENTER_ENTRIES[APP_CENTER_ENTRY_COUNT] = {
{ "Fichiers", DESKTOP_ACTION_FILES, cmd_files },
{ "Calculatrice", DESKTOP_ACTION_CALC, cmd_calc },
{ "Parametres", DESKTOP_ACTION_SETTINGS, cmd_settings },
{ "Compte", DESKTOP_ACTION_ACCOUNT, cmd_gui },
{ "Corbeille", DESKTOP_ACTION_TRASH, cmd_trash_app },
{ "Terminal", DESKTOP_ACTION_TERMINAL, (void*)0 },
{ "Installer sur le disque", DESKTOP_ACTION_INSTALL, (void*)0 },
{ "Deconnexion", DESKTOP_ACTION_LOGOUT, (void*)0 },
{ "Redemarrer", DESKTOP_ACTION_REBOOT, (void*)0 },
{ "Eteindre", DESKTOP_ACTION_SHUTDOWN, (void*)0 }
};


static void icon_draw_for_action(desktop_action action, u32 px, u32 py, u32 size, gfx_color color)
{

switch (action)
{

case DESKTOP_ACTION_FILES: icon_draw_folder(px, py, size, color); break;

case DESKTOP_ACTION_CALC: icon_draw_calculator(px, py, size, color); break;

case DESKTOP_ACTION_SETTINGS: icon_draw_settings(px, py, size, color); break;

case DESKTOP_ACTION_ACCOUNT: icon_draw_user(px, py, size, color); break;

case DESKTOP_ACTION_TRASH: icon_draw_trash(px, py, size, color); break;

case DESKTOP_ACTION_TERMINAL: icon_draw_terminal(px, py, size, color); break;

case DESKTOP_ACTION_LOGOUT: icon_draw_power(px, py, size, color); break;

case DESKTOP_ACTION_REBOOT: icon_draw_reboot(px, py, size, color); break;

case DESKTOP_ACTION_SHUTDOWN: icon_draw_power(px, py, size, color); break;

case DESKTOP_ACTION_INSTALL: icon_draw_disk(px, py, size, color); break;

}

}


/*
    Extinction/redemarrage (kernel/drivers/power) : ecran dedie
    (efface l'ecran, affiche le resultat) plutot qu'un appel
    direct depuis le gestionnaire de clic -- si l'extinction
    echoue (voir power_shutdown(), materiel non reconnu), le
    message d'explication doit rester lisible le temps que
    l'utilisateur appuie sur Entree, au lieu de disparaitre
    aussitot sous le prochain redessin du bureau.
*/

static void desktop_run_reboot()
{

console_clear();

power_reboot();

/* Ne revient jamais si power_reboot() reussit. */

}


static void desktop_run_shutdown()
{

console_clear();

desktop_draw_console_titlebar("Extinction");

console_set_top_margin(1);

power_shutdown();

console_write("\nAppuyez sur Entree pour revenir au bureau.\n");

keyboard_flush();

char dummy[8];

input_readline(dummy, sizeof(dummy));


console_set_top_margin(0);

}


/*
    Installateur (shell/commands.c, cmd_install() -- deja
    reserve a root et deja protege par une confirmation "OUI"
    tapee au clavier) : reutilise tel quel via execute_command()
    plutot que de reimplementer une boite de dialogue graphique
    specifique pour une action aussi destructrice (efface le
    disque de donnees).
*/

static void desktop_run_installer()
{

console_clear();

desktop_draw_console_titlebar("Installateur");

console_set_top_margin(1);


char cmd[16] = "install";

execute_command(cmd);


console_write("\nAppuyez sur Entree pour revenir au bureau.\n");

keyboard_flush();

char dummy[8];

input_readline(dummy, sizeof(dummy));


console_set_top_margin(0);

}


/*
    Traite une action SYSTEME (jamais une application-fenetre,
    voir "window_entry" ci-dessus, gerees separement au point
    d'appel). Renvoie 1 si gui_desktop_run() doit se terminer
    (deconnexion), 0 sinon.
*/

static int desktop_run_system_action(desktop_action action)
{

switch (action)
{

case DESKTOP_ACTION_TERMINAL:

gui_cursor_erase();

return desktop_run_terminal() ? 1 : 0;

case DESKTOP_ACTION_LOGOUT: gui_cursor_erase(); return 1;

case DESKTOP_ACTION_REBOOT: gui_cursor_erase(); desktop_run_reboot(); return 0;

case DESKTOP_ACTION_SHUTDOWN: gui_cursor_erase(); desktop_run_shutdown(); return 0;

case DESKTOP_ACTION_INSTALL: gui_cursor_erase(); desktop_run_installer(); return 0;

default: return 0;

}

}


/*
    Etat complet du bureau : regroupe tout ce que desktop_draw()
    calcule et que gui_desktop_run() doit ensuite tester au clic
    -- un seul parametre a faire circuler plutot qu'une liste
    croissante de tableaux/variables de sortie independants.
*/

typedef struct
{

desktop_hitbox apps_button;

desktop_hitbox window_buttons[GUI_MAX_WINDOWS];

int app_center_open;

desktop_hitbox app_center_entries[APP_CENTER_ENTRY_COUNT];

desktop_hitbox app_center_panel;

} desktop_state;


/*
    Barre des taches : une icone fixe ("Toutes les
    applications", grille 3x3, meme motif que le bouton de
    demarrage de Windows 11 ou le tiroir d'applications GNOME/
    Android), puis UNE LIGNE PAR FENETRE ACTUELLEMENT OUVERTE
    (voir gui/window.c) -- qu'elle soit reduite ou non. Cliquer
    dessus lui redonne le focus (la restaure si elle etait
    reduite). C'est ce qui remplace l'ancienne liste fixe
    d'icones : le nombre de boutons reflete desormais ce qui
    tourne reellement, pas une liste figee d'applications
    possibles.
*/

static void desktop_draw_taskbar(desktop_state* state)
{


u32 cols = desktop_cols();

u32 rows = desktop_rows();


int taskbar_h = 2;

int taskbar_y = (int)rows - taskbar_h;


u32 tpx = 0;

u32 tpy = (u32)taskbar_y * 8 * GFX_SCALE;

u32 tpw = cols * 8 * GFX_SCALE;

u32 tph = (u32)taskbar_h * 8 * GFX_SCALE;


gfx_fill_rect_blend(tpx, tpy, tpw, tph, theme_titlebar_bg(), 92);

gfx_draw_hline(tpx, tpy, tpw, theme_border());


u32 button_size = (u32)taskbar_h * 8 * GFX_SCALE;

u32 icon_padding = button_size / 5;

u32 icon_size = button_size - 2 * icon_padding;


/* Bouton "Toutes les applications". */

gfx_fill_rect_blend(0, tpy, button_size, tph, theme_button_bg(), 92);

icon_draw_grid(icon_padding, tpy + icon_padding, icon_size, theme_text());

state->apps_button.x = 0;

state->apps_button.y = tpy;

state->apps_button.w = button_size;

state->apps_button.h = tph;


gfx_draw_vline(button_size, tpy, tph, theme_border());


/*
    Une ligne par fenetre ouverte, dans l'ordre des slots (voir
    gui/window.c). "win_col" avance en cellules de texte, pas
    en pixels (gui_draw_button() attend des cellules) --
    s'arrete avant d'empieter sur l'horloge si trop de fenetres
    sont ouvertes en meme temps (degradation propre plutot
    qu'un chevauchement illisible).
*/

int win_col = (int)(button_size / (8 * GFX_SCALE)) + 1;

int win_btn_w = 8;

int clock_reserved = 7; /* "HH:MM" + marge */


for (int i = 0; i < GUI_MAX_WINDOWS; i++)
{


state->window_buttons[i].w = 0;


const gui_window_slot* w = gui_window_get(i);

if (w == 0)
{

continue;

}


if (win_col + win_btn_w > (int)cols - clock_reserved)
{

/* Plus de place : les fenetres suivantes n'apparaissent pas dans la barre (rare, ecran tres etroit ou tres nombreuses fenetres). */

break;

}


char label[10];

int k = 0;

while (w->title[k] != 0 && k < 8)
{

label[k] = w->title[k];

k++;

}

label[k] = 0;


u32 bx, by, bw, bh;

gui_draw_button(win_col, taskbar_y, win_btn_w, taskbar_h, label, &bx, &by, &bw, &bh);


/*
    Petit repere sous le libelle pour distinguer une fenetre
    reduite (attend en arriere-plan) d'une fenetre active --
    simple trait, pas une couleur differente (voir gui/theme.c,
    un seul accent de couleur dans tout le theme).
*/

if (!w->minimized)
{

gfx_draw_hline(bx + 2, by + bh - 3, bw - 4, theme_text());

}


state->window_buttons[i].x = bx;

state->window_buttons[i].y = by;

state->window_buttons[i].w = bw;

state->window_buttons[i].h = bh;


win_col += win_btn_w + 1;


}


/* Horloge, ancree a droite. */

char clock_text[16];

format_walltime(clock_text);

int clock_w = 5; /* "HH:MM" */

int clock_x = (int)cols - clock_w - 1;

if (clock_x - 1 > win_col)
{

/*
    gui_draw_text() aligne toujours le texte sur le HAUT de
    sa cellule -- sur une barre haute de 2 cellules, ca
    laissait l'horloge collee en haut avec un vide en
    dessous. Position pixel calculee directement pour un
    centrage vertical exact, comme gui_draw_field().
*/

u32 clock_px = (u32)clock_x * 8 * GFX_SCALE;

u32 clock_py = tpy + (tph - 8 * GFX_SCALE) / 2;

gfx_draw_text(clock_px, clock_py, clock_text, theme_titlebar_text(), GFX_SCALE);

}

}


#define APP_CENTER_ROW_H 2

#define APP_CENTER_WIDTH 22


/*
    Centre d'applications : panneau "verre depoli" ancre juste
    au-dessus du bouton "Toutes les applications", meme
    principe que le menu Demarrer de Windows 11 ou le tiroir
    d'applications GNOME -- une icone + un libelle par entree,
    les actions systeme separees du reste par une ligne fine.
*/

static void desktop_draw_app_center(desktop_state* state)
{


u32 rows = desktop_rows();

int taskbar_y = (int)rows - 2;


int panel_h = APP_CENTER_ENTRY_COUNT * APP_CENTER_ROW_H + 2;

int panel_x = 1;

int panel_y = taskbar_y - panel_h;

if (panel_y < 1)
{

panel_y = 1;

}


u32 ppx = (u32)panel_x * 8 * GFX_SCALE;

u32 ppy = (u32)panel_y * 8 * GFX_SCALE;

u32 ppw = (u32)APP_CENTER_WIDTH * 8 * GFX_SCALE;

u32 pph = (u32)panel_h * 8 * GFX_SCALE;


gfx_fill_rect_blend(ppx + 3, ppy + 3, ppw, pph, theme_shadow(), 20);

gfx_fill_rect_blend(ppx, ppy, ppw, pph, theme_panel(), 96);

gfx_draw_rect(ppx, ppy, ppw, pph, theme_border());


state->app_center_panel.x = ppx;

state->app_center_panel.y = ppy;

state->app_center_panel.w = ppw;

state->app_center_panel.h = pph;


u32 icon_size = 8 * GFX_SCALE;


for (int i = 0; i < APP_CENTER_ENTRY_COUNT; i++)
{

u32 row_py = ppy + (u32)(1 + i * APP_CENTER_ROW_H) * 8 * GFX_SCALE;

u32 row_h = (u32)APP_CENTER_ROW_H * 8 * GFX_SCALE;


if (APP_CENTER_ENTRIES[i].action == DESKTOP_ACTION_TERMINAL)
{

gfx_draw_hline(ppx + 4, row_py, ppw - 8, theme_border());

}


icon_draw_for_action(APP_CENTER_ENTRIES[i].action, ppx + 8, row_py + (row_h - icon_size) / 2, icon_size, theme_text());

gfx_draw_text(ppx + 8 + icon_size + 8, row_py + (row_h - 8 * GFX_SCALE) / 2, APP_CENTER_ENTRIES[i].label, theme_text(), GFX_SCALE);


state->app_center_entries[i].x = ppx;

state->app_center_entries[i].y = row_py;

state->app_center_entries[i].w = ppw;

state->app_center_entries[i].h = row_h;

}

}


/*
    "Papier peint" procedural : ce projet n'a aucun decodeur
    d'image (ni PNG, ni BMP -- en ajouter un est un chantier a
    part entiere, hors de portee raisonnable ici), donc pas de
    veritable photo a afficher. A la place, 6 motifs distincts
    au choix (voir gui/theme.h, wallpaper_style -- reglable
    depuis apps/settings/settings.c), dessines dans un
    rectangle arbitraire ("x","y","w","h") plutot que
    forcement plein ecran : reutilisee telle quelle pour les
    vignettes miniatures du selecteur de Parametres.
*/

void gui_paint_wallpaper_area(u32 x, u32 y, u32 w, u32 h, wallpaper_style style)
{

gfx_color base = theme_desktop_bg();

int base_r = (int)((base >> 16) & 0xFF);

int base_g = (int)((base >> 8) & 0xFF);

int base_b = (int)(base & 0xFF);


switch (style)
{


case WALLPAPER_GRADIENT:
{

int total_shift = 12;

u32 bands = 24;

u32 band_h = (h / bands) + 1;

for (u32 i = 0; i < bands; i++)
{

int delta = -(int)((total_shift * (long)i) / (long)bands);

int r = base_r + delta; if (r < 0) { r = 0; } if (r > 255) { r = 255; }

int g = base_g + delta; if (g < 0) { g = 0; } if (g > 255) { g = 255; }

int b = base_b + delta; if (b < 0) { b = 0; } if (b > 255) { b = 255; }

gfx_color band_color = ((u32)r << 16) | ((u32)g << 8) | (u32)b;

gfx_fill_rect(x, y + i * band_h, w, band_h, band_color);

}

break;

}


case WALLPAPER_STRIPES:
{

gfx_fill_rect(x, y, w, h, base);

u32 stripe_h = (h / 16) + 1;

for (u32 i = 0; i < h; i += stripe_h * 2)
{

gfx_fill_rect_blend(x, y + i, w, stripe_h, theme_shadow(), 8);

}

break;

}


case WALLPAPER_CHECKER:
{

gfx_fill_rect(x, y, w, h, base);

u32 cell = (w / 8) + 1;

int toggle_row = 0;

for (u32 cy = 0; cy < h; cy += cell)
{

int toggle = toggle_row;

for (u32 cx = 0; cx < w; cx += cell)
{

if (toggle)
{

u32 cw = (cx + cell <= w) ? cell : (w - cx);

u32 ch = (cy + cell <= h) ? cell : (h - cy);

gfx_fill_rect_blend(x + cx, y + cy, cw, ch, theme_shadow(), 6);

}

toggle = !toggle;

}

toggle_row = !toggle_row;

}

break;

}


case WALLPAPER_DOTS:
{

gfx_fill_rect(x, y, w, h, base);

u32 spacing = (w / 12) + 1;

u32 dot = spacing / 4; if (dot < 1) { dot = 1; }

for (u32 dy = spacing / 2; dy < h; dy += spacing)
{

for (u32 dx = spacing / 2; dx < w; dx += spacing)
{

gfx_fill_rect_blend(x + dx, y + dy, dot, dot, theme_shadow(), 18);

}

}

break;

}


case WALLPAPER_DIAGONAL:
{

gfx_fill_rect(x, y, w, h, base);

u32 step = (w / 10) + 1;

for (u32 i = 0; i < w + h; i += step)
{

u32 lx = (i < w) ? (x + i) : x;

u32 ly = (i < w) ? y : (y + (i - w));

u32 len = (h < w) ? h : w;

if (lx + len > x + w) { len = (x + w > lx) ? (x + w - lx) : 0; }

if (ly + len > y + h) { len = (y + h > ly) ? (y + h - ly) : 0; }

for (u32 k = 0; k < len; k++)
{

if (lx + k < x + w && ly + k < y + h)
{

gfx_put_pixel(lx + k, ly + k, theme_border());

}

}

}

break;

}


default: /* WALLPAPER_SOLID */
{

gfx_fill_rect(x, y, w, h, base);

break;

}


}

}


static void desktop_paint_wallpaper()
{

gui_paint_wallpaper_area(0, 0, gfx_width(), gfx_height(), wallpaper_get_style());

}


static void desktop_draw(desktop_state* state)
{


console_clear();


if (gfx_available())
{

desktop_paint_wallpaper();

}


desktop_draw_taskbar(state);


if (state->app_center_open)
{

desktop_draw_app_center(state);

}

}


void desktop_render_backdrop()
{

if (!gfx_available())
{

return;

}


desktop_state state;

state.app_center_open = 0;


console_clear();

desktop_paint_wallpaper();

desktop_draw_taskbar(&state);

}


void gui_desktop_run()
{


if (!gfx_available())
{

/*
    Garde-fou : shell/msh.c ne devrait jamais appeler cette
    fonction sans mode graphique disponible (voir la
    condition dans msh_start()), mais on evite quand meme
    tout rendu pixel invalide si c'etait le cas.
*/

return;

}


desktop_state state;

state.app_center_open = 0;


desktop_draw(&state);

gui_cursor_reset();


keyboard_flush();


int was_pressed = 0;

int had_focus = 1;


unsigned long last_clock_update = timer_ticks();


while (1)
{


/*
    Jeton de focus (voir gui/window.c) : des qu'une fenetre
    d'application a le focus, le bureau reste en pause --
    ne dessine rien, ne lit ni la souris ni le clavier --
    pour eviter tout conflit d'affichage avec la fenetre
    active. gui_window_idle() cede volontairement le CPU
    plutot que d'attendre la prochaine preemption materielle.
*/

if (!gui_window_desktop_has_focus())
{

had_focus = 0;

gui_window_idle();

continue;

}


if (!had_focus)
{

/*
    On vient de regagner le focus (fenetre fermee ou
    reduite) : tout redessiner, l'ecran affiche encore le
    contenu de cette fenetre.
*/

desktop_draw(&state);

gui_cursor_reset();

keyboard_flush();


if (gui_window_take_pending_click())
{

/*
    Correctif (barre des taches non cliquable pendant
    qu'une fenetre est ouverte) : une application vient
    de nous ceder un clic deja en cours (voir
    gui_window_yield_click(), gui/window.c) -- on force
    "was_pressed" a 0 pour que le traitement de clic
    normal ci-dessous le reconnaisse IMMEDIATEMENT comme
    un nouveau clic, sans obliger l'utilisateur a
    relacher puis re-cliquer.
*/

was_pressed = 0;

}
else
{

/*
    Meme precaution que pour un clic transfere : capturer
    l'etat REEL du bouton plutot que de supposer qu'il
    est relache, pour ne pas confondre un clic deja en
    cours ailleurs avec un nouveau clic sur le bureau.
*/

was_pressed = mouse_left_pressed();

}


had_focus = 1;

}


/*
    Correctif CRITIQUE (deconnexion sans effet depuis
    Parametres) : "Deconnexion" (apps/settings/settings.c)
    tourne desormais dans son PROPRE thread (voir gui/window.c)
    -- shell_request_logout() y pose bien le meme drapeau que
    partout ailleurs, mais seul CE thread-ci (celui qui execute
    gui_desktop_run(), le meme que msh_start()) est en mesure de
    le lire et d'agir en consequence (voir shell/msh.c,
    msh_start()). Sans cette verification explicite, fermer la
    fenetre Parametres apres avoir clique "Deconnexion" rendait
    simplement le focus au bureau, sans jamais reellement
    deconnecter personne. Verifiee a chaque tour (pas seulement
    apres un changement de focus), pour couvrir aussi le cas du
    Terminal integre (desktop_run_terminal(), qui gere deja ce
    meme drapeau lui-meme mais par un chemin different).
*/

if (shell_logout_was_requested())
{

return;

}


s32 mx = mouse_get_x();

s32 my = mouse_get_y();

gui_draw_cursor(mx, my);


int now_pressed = mouse_left_pressed();

int clicked = (now_pressed && !was_pressed);

was_pressed = now_pressed;


if (clicked)
{


if (state.app_center_open)
{


/*
    Centre d'applications ouvert : n'importe quel clic le
    referme -- soit parce qu'il vient d'activer une
    entree, soit parce qu'il a clique en dehors pour
    l'annuler.
*/

int matched = -1;

for (int i = 0; i < APP_CENTER_ENTRY_COUNT; i++)
{

if (gui_point_in_rect(
state.app_center_entries[i].x, state.app_center_entries[i].y,
state.app_center_entries[i].w, state.app_center_entries[i].h,
mx, my
))
{

matched = i;

break;

}

}


state.app_center_open = 0;


if (matched >= 0)
{


if (APP_CENTER_ENTRIES[matched].window_entry != 0)
{


/*
    Vraie application : lancee comme une fenetre
    independante (voir gui/window.c), qui recoit
    immediatement le focus -- le bureau se remet en
    pause au prochain tour de cette boucle (voir le
    garde-fou tout en haut). N'est pas bloquant :
    gui_window_open() ne fait que creer le thread et
    rendre la main immediatement.
*/

int opened = gui_window_open(APP_CENTER_ENTRIES[matched].label, APP_CENTER_ENTRIES[matched].window_entry);


if (opened < 0)
{

sound_play_error();


/*
    Correctif (clic sans aucun effet visible) : table de
    fenetres pleine (GUI_MAX_WINDOWS, voir gui/window.h)
    ou plus assez de memoire pour la pile du nouveau
    thread -- sans ce message, le clic semblait
    silencieusement ignore, rien n'indiquait que
    l'ouverture avait echoue. Le bureau garde le focus
    dans ce cas (gui_window_open() ne l'a pas transfere).
    Le message est dessine APRES desktop_draw() (qui
    efface l'ecran en premiere etape) pour ne pas etre
    aussitot recouvert.
*/

desktop_draw(&state);

gui_draw_text(1, 3, "Impossible d'ouvrir : trop de fenetres deja ouvertes.", theme_text_attr());

gui_cursor_reset();

keyboard_flush();

was_pressed = 0;

}

}
else
{


if (desktop_run_system_action(APP_CENTER_ENTRIES[matched].action))
{

return;

}


desktop_draw(&state);

gui_cursor_reset();

keyboard_flush();

was_pressed = 0;


}

}
else
{

desktop_draw(&state);

gui_cursor_reset();

keyboard_flush();

was_pressed = 0;

}

}

else if (gui_point_in_rect(state.apps_button.x, state.apps_button.y, state.apps_button.w, state.apps_button.h, mx, my))
{

state.app_center_open = 1;

desktop_draw(&state);

gui_cursor_reset();

}

else
{


int matched_window = -1;

for (int i = 0; i < GUI_MAX_WINDOWS; i++)
{

if (state.window_buttons[i].w > 0 && gui_point_in_rect(
state.window_buttons[i].x, state.window_buttons[i].y,
state.window_buttons[i].w, state.window_buttons[i].h,
mx, my
))
{

matched_window = i;

break;

}

}


if (matched_window >= 0)
{

/*
    Redonne le focus a cette fenetre (la restaure si
    elle etait reduite) -- le bureau se remet en pause
    au prochain tour, exactement comme apres un
    lancement depuis le Centre d'applications.
*/

gui_window_focus(matched_window);

}

}

}


/*
    Raccourci clavier : Entree ouvre directement le terminal
    integre (equivalent au clic sur son icone dans le Centre
    d'applications), pour qui prefere le clavier a la souris.
*/

if (!state.app_center_open && keyboard_available())
{

char c = keyboard_getchar();

if (c == '\n')
{

if (desktop_run_system_action(DESKTOP_ACTION_TERMINAL))
{

return;

}

desktop_draw(&state);

gui_cursor_reset();

keyboard_flush();

was_pressed = 0;

}

}


if (!state.app_center_open)
{

unsigned long now = timer_ticks();

if (now - last_clock_update >= 500)
{

last_clock_update = now;


/*
    Correctif (scintillement visible toutes les 5 secondes
    meme sans rien taper) : la version precedente appelait
    desktop_draw(&state), qui efface et repeint l'ecran
    ENTIER (fond degrade compris, l'operation la plus
    couteuse de ce fichier) juste pour mettre a jour 5
    caracteres d'horloge. On ne redessine desormais que la
    bande de la barre des taches (desktop_draw_taskbar()),
    largement suffisant puisque c'est la seule chose qui
    change. gui_cursor_erase() avant (pas
    gui_cursor_reset()) : contrairement a un console_clear()
    complet, l'ecran n'est pas efface ailleurs, donc le
    tampon de restauration du curseur reste valable -- il
    suffit de retirer le curseur avant de repeindre en
    dessous, il sera resauvegarde normalement au prochain
    gui_draw_cursor().
*/

gui_cursor_erase();

desktop_draw_taskbar(&state);

}

}


unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{
}


}


}
