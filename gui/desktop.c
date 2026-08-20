#include "desktop.h"

#include "gui.h"

#include "theme.h"

#include "icons.h"

#include "../kernel/drivers/graphics/graphics.h"

#include "../kernel/drivers/mouse/mouse.h"

#include "../kernel/drivers/rtc/rtc.h"


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
    l'horloge de fonctionnement ci-dessous). Pas de sprintf en
    freestanding : meme esprit que write_int_buf() dans
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
    "HH:MM" -- horloge murale reelle (kernel/drivers/rtc),
    plutot que le temps ecoule depuis le demarrage : lit la
    puce CMOS du PC (ou de l'emulateur, ex. QEMU la fournit
    aussi), comme le ferait n'importe quel bureau habituel.
    Pas de fuseau horaire gere (voir le commentaire de rtc.h) :
    c'est l'heure telle que configuree dans la RTC (UTC par
    defaut sous QEMU sans l'option "-rtc base=localtime").
    Pas de secondes affichees : superflu pour une horloge de
    barre des taches (aucun bureau habituel n'en affiche), et
    ca evite un rafraichissement toutes les secondes pour rien.
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
    les reimplementer -- seule la boucle d'invite change de
    forme (fenetre de bureau plutot que console plein ecran au
    demarrage). "exit" (propre a cette fenetre, pas une
    commande shell existante) revient simplement au bureau ;
    "logout" (commande shell existante) est detectee via
    shell_logout_was_requested() pour remonter la demande de
    deconnexion jusqu'a gui_desktop_run().
*/

static int desktop_run_terminal()
{

char command[128];


console_clear();

console_write("Terminal Mikea OS\n");

console_write("Tapez 'help' pour la liste des commandes, 'exit' pour revenir au bureau.\n\n");


keyboard_flush();


while (1)
{

console_write("$ ");

input_readline(command, sizeof(command));


if (mk_strcmp(command, "exit") == 0)
{

return 0;

}


execute_command(command);


if (shell_logout_was_requested())
{

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
    Actions possibles depuis la barre des taches OU le Centre
    d'applications (voir plus bas) : les deux partagent le meme
    lanceur (desktop_launch()) plutot que de dupliquer la
    logique de clic pour chaque emplacement possible d'une
    meme action.
*/

typedef enum
{

DESKTOP_ACTION_FILES,

DESKTOP_ACTION_CALC,

DESKTOP_ACTION_SETTINGS,

DESKTOP_ACTION_ACCOUNT,

DESKTOP_ACTION_TERMINAL,

DESKTOP_ACTION_LOGOUT

} desktop_action;


#define APP_CENTER_ENTRY_COUNT 6


typedef struct
{

const char* label;

desktop_action action;

} app_center_entry;


static const app_center_entry APP_CENTER_ENTRIES[APP_CENTER_ENTRY_COUNT] = {
{ "Fichiers", DESKTOP_ACTION_FILES },
{ "Calculatrice", DESKTOP_ACTION_CALC },
{ "Parametres", DESKTOP_ACTION_SETTINGS },
{ "Terminal", DESKTOP_ACTION_TERMINAL },
{ "Compte", DESKTOP_ACTION_ACCOUNT },
{ "Deconnexion", DESKTOP_ACTION_LOGOUT }
};


static void icon_draw_for_action(desktop_action action, u32 px, u32 py, u32 size, gfx_color color)
{

switch (action)
{

case DESKTOP_ACTION_FILES: icon_draw_folder(px, py, size, color); break;

case DESKTOP_ACTION_CALC: icon_draw_calculator(px, py, size, color); break;

case DESKTOP_ACTION_SETTINGS: icon_draw_settings(px, py, size, color); break;

case DESKTOP_ACTION_ACCOUNT: icon_draw_user(px, py, size, color); break;

case DESKTOP_ACTION_TERMINAL: icon_draw_terminal(px, py, size, color); break;

case DESKTOP_ACTION_LOGOUT: icon_draw_power(px, py, size, color); break;

}

}


/*
    Renvoie 1 si "action" doit terminer gui_desktop_run() (donc
    reellement deconnecter l'utilisateur), 0 sinon -- c'est
    l'appelant (gui_desktop_run()) qui gere le retour, cette
    fonction se contente de lancer l'application demandee de
    facon bloquante (meme modele que les anciens boutons de
    barre des taches).
*/

static int desktop_launch(desktop_action action)
{

switch (action)
{

case DESKTOP_ACTION_FILES: gui_cursor_erase(); cmd_files(); return 0;

case DESKTOP_ACTION_CALC: gui_cursor_erase(); cmd_calc(); return 0;

case DESKTOP_ACTION_SETTINGS: gui_cursor_erase(); cmd_settings(); return 0;

case DESKTOP_ACTION_ACCOUNT: gui_cursor_erase(); cmd_gui(); return 0;

case DESKTOP_ACTION_TERMINAL:

gui_cursor_erase();

return desktop_run_terminal() ? 1 : 0;

case DESKTOP_ACTION_LOGOUT: gui_cursor_erase(); return 1;

}

return 0;

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

desktop_hitbox terminal_button;

int app_center_open;

desktop_hitbox app_center_entries[APP_CENTER_ENTRY_COUNT];

desktop_hitbox app_center_panel;

} desktop_state;


/*
    Barre des taches simplifiee (etape suivante) : au lieu
    d'une icone par application alignees les unes a cote des
    autres (encombre vite l'ecran des qu'on ajoute une
    application), seules DEUX icones restent en permanence
    visibles -- "Toutes les applications" (grille 3x3, meme
    motif que le bouton de demarrage de Windows 11 ou le tiroir
    d'applications GNOME/Android) et "Terminal" (acces rapide
    conserve pour des raisons d'ergonomie -- c'est l'outil le
    plus utilise en dehors des applications graphiques). Toutes
    les autres actions (Fichiers, Calculatrice, Parametres,
    Compte, Deconnexion) vivent desormais dans le panneau
    "Centre d'applications" ouvert par la premiere icone -- y
    compris Terminal, qui y figure aussi (acces double,
    volontaire).
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


/* Bouton "Terminal", juste apres, meme gabarit. */

u32 term_x = button_size;

gfx_fill_rect_blend(term_x, tpy, button_size, tph, theme_button_bg(), 92);

icon_draw_terminal(term_x + icon_padding, tpy + icon_padding, icon_size, theme_text());

state->terminal_button.x = term_x;

state->terminal_button.y = tpy;

state->terminal_button.w = button_size;

state->terminal_button.h = tph;


gfx_draw_vline(2 * button_size, tpy, tph, theme_border());


/* Horloge de fonctionnement, ancree a droite. */

char clock_text[16];

format_walltime(clock_text);

int clock_w = 5; /* "HH:MM" */

int clock_x = (int)cols - clock_w - 1;

int buttons_end_col = (int)(2 * button_size / (8 * GFX_SCALE));

if (clock_x - 1 > buttons_end_col)
{

gui_draw_text(clock_x, taskbar_y, clock_text, theme_titlebar_text());

}

}


#define APP_CENTER_ROW_H 2

#define APP_CENTER_WIDTH 22


/*
    Centre d'applications : panneau "verre depoli" ancre juste
    au-dessus du bouton "Toutes les applications", meme
    principe que le menu Demarrer de Windows 11 ou le tiroir
    d'applications GNOME -- une icone + un libelle par entree,
    "Deconnexion" separee du reste par une ligne fine (action
    systeme plutot qu'une application).
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


/*
    Separateur avant "Deconnexion" (derniere entree) : c'est
    une action systeme, pas une application, meme
    distinction visuelle que le bas du menu Demarrer de
    Windows ou du menu utilisateur de GNOME.
*/

if (APP_CENTER_ENTRIES[i].action == DESKTOP_ACTION_LOGOUT)
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
    veritable image de fond a afficher. A defaut, un degrade
    vertical doux (legerement plus sombre en bas qu'en haut)
    plutot qu'un simple aplat uni -- meme esprit que les fonds
    d'ecran par defaut de Windows 11 ou GNOME, en beaucoup plus
    simple : uniquement des bandes horizontales de
    gfx_fill_rect(), sans le cout d'un degrade pixel par pixel.
*/

static void desktop_paint_wallpaper()
{

u32 w = gfx_width();

u32 h = gfx_height();


gfx_color base = theme_desktop_bg();

int base_r = (int)((base >> 16) & 0xFF);

int base_g = (int)((base >> 8) & 0xFF);

int base_b = (int)(base & 0xFF);


int total_shift = 12;

u32 bands = 48;

u32 band_h = (h / bands) + 1;


for (u32 i = 0; i < bands; i++)
{

int delta = -(int)((total_shift * (long)i) / (long)bands);


int r = base_r + delta; if (r < 0) { r = 0; } if (r > 255) { r = 255; }

int g = base_g + delta; if (g < 0) { g = 0; } if (g > 255) { g = 255; }

int b = base_b + delta; if (b < 0) { b = 0; } if (b > 255) { b = 255; }


gfx_color band_color = ((u32)r << 16) | ((u32)g << 8) | (u32)b;

gfx_fill_rect(0, i * band_h, w, band_h, band_color);

}

}


static void desktop_draw(desktop_state* state)
{


console_clear();


if (gfx_available())
{

desktop_paint_wallpaper();

}


/*
    Bureau : un petit "brandage" discret en haut a gauche,
    comme le nom du bureau qu'on voit parfois en haut a
    gauche sous GNOME/macOS.
*/

gui_draw_text(1, 1, "Mikea OS", theme_text_attr());


desktop_draw_taskbar(state);


if (state->app_center_open)
{

desktop_draw_app_center(state);

}

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


/*
    Rafraichit l'horloge environ toutes les 5 secondes (500
    ticks a PIT_FREQUENCY=100Hz, voir
    kernel/drivers/timer/timer.c) plutot qu'a chaque image :
    l'affichage n'a que la precision de la minute (voir
    format_walltime() ci-dessus), donc un rafraichissement
    seconde par seconde gaspillerait du temps CPU pour rien --
    5 secondes de retard maximum sur le changement de minute
    reste largement imperceptible pour une horloge de barre des
    taches. Sans effet pendant que le Centre d'applications est
    ouvert (son propre affichage prend le pas, voir plus bas).
*/

unsigned long last_clock_update = timer_ticks();


while (1)
{


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
    entree (comportement standard d'un menu contextuel),
    soit parce qu'il a clique en dehors pour l'annuler
    (aucune action a effectuer dans ce cas, juste le
    fermer).
*/

int matched_action = -1;

for (int i = 0; i < APP_CENTER_ENTRY_COUNT; i++)
{

if (gui_point_in_rect(
state.app_center_entries[i].x, state.app_center_entries[i].y,
state.app_center_entries[i].w, state.app_center_entries[i].h,
mx, my
))
{

matched_action = i;

break;

}

}


state.app_center_open = 0;


if (matched_action >= 0)
{

if (desktop_launch(APP_CENTER_ENTRIES[matched_action].action))
{

return;

}

}


desktop_draw(&state);

gui_cursor_reset();

keyboard_flush();

was_pressed = 0;

}

else if (gui_point_in_rect(state.apps_button.x, state.apps_button.y, state.apps_button.w, state.apps_button.h, mx, my))
{

state.app_center_open = 1;

desktop_draw(&state);

gui_cursor_reset();

}

else if (gui_point_in_rect(state.terminal_button.x, state.terminal_button.y, state.terminal_button.w, state.terminal_button.h, mx, my))
{

if (desktop_launch(DESKTOP_ACTION_TERMINAL))
{

return;

}

desktop_draw(&state);

gui_cursor_reset();

keyboard_flush();

was_pressed = 0;

}

}


/*
    Raccourci clavier : Entree ouvre directement le terminal
    integre (equivalent au clic sur son icone), pour qui
    prefere le clavier a la souris -- comme Windows (touche
    Windows puis taper) ou GNOME (Activites).
*/

if (!state.app_center_open && keyboard_available())
{

char c = keyboard_getchar();

if (c == '\n')
{

if (desktop_launch(DESKTOP_ACTION_TERMINAL))
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

desktop_draw(&state);

gui_cursor_reset();

}

}


unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{
}


}


}
