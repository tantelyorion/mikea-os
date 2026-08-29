#include "desktop.h"

#include "gui.h"

#include "theme.h"

#include "icons.h"

#include "window.h"

#include "assets/wallpaper_images.h"

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

void cmd_terminal_app();

void cmd_installer_app();


void console_set_top_margin(int rows);

void console_set_fg_override(gfx_color color, int enabled);

void console_set_bg_override(gfx_color color, int enabled);


static void desktop_draw_console_titlebar(const char* title);

static void desktop_reset_console_colors();


#define GFX_SCALE 2


/*
    Correctif (interface macOS) : desktop_cols()/desktop_rows()
    (nombre de cellules texte 8x8 a l'ecran) servaient a
    positionner l'ancienne barre des taches/le Centre
    d'applications en unites de cellules. Le nouveau Dock/
    Launchpad et la barre de menu (voir desktop_draw_taskbar(),
    desktop_draw_menubar(), desktop_draw_app_center() plus bas)
    calculent tous leurs positionnements directement en pixels
    (gfx_width()/gfx_height()) pour un centrage exact -- ces deux
    fonctions n'ont plus d'appelant.
*/


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


typedef struct
{

u32 x, y, w, h;

} desktop_hitbox;


/*
    Cadre "console plein ecran" pour les applications texte
    (Terminal, Installateur, Extinction) : contrairement aux
    fenetres "normales" (voir gui/gui.c, gui_draw_window()), ces
    applications utilisent la console texte (console_write()/
    input_readline(), kernel/console/console.c) plutot qu'un
    rendu image par image avec sondage de la souris -- les
    integrer au meme systeme de fenetres a bouton fermer/reduire/
    agrandir cliquables exigerait de reecrire leur lecture
    d'entree de zero (voir le commentaire de
    desktop_run_terminal() plus bas).

    A defaut, ce cadre leur donne quand meme une VRAIE identite
    de fenetre : fond NOIR plein ecran (comme n'importe quel
    terminal reel, independant du theme clair/sombre courant --
    voir console_set_bg_override(), kernel/console/console.c) et
    une barre de titre, avec le texte qui defile EN DESSOUS grace
    a console_set_top_margin() -- jamais par-dessus.

    Limite assumee : si le texte affiche utilise la commande
    shell "clear" (efface tout, y compris cette bande), la barre
    de titre disparait jusqu'a la prochaine ouverture -- cas
    marginal, non traite ici pour rester dans un temps
    raisonnable.
*/

static void desktop_draw_console_titlebar(const char* title)
{

u32 bar_w = gfx_width();

u32 bar_h = 8 * GFX_SCALE + 4;


/*
    Fond noir plein ecran (pas la couleur du theme) : c'est ce
    qui donne au Terminal/Installateur leur apparence de
    "vraie" fenetre de console, plutot qu'un texte flottant sur
    le fond degrade/photo du bureau.
*/

gfx_fill_rect(0, 0, gfx_width(), gfx_height(), 0x000000);


gfx_fill_rect_blend(0, 0, bar_w, bar_h, theme_titlebar_bg(), 95);

gfx_draw_hline(0, bar_h - 1, bar_w, theme_border());

gfx_draw_text(8 * GFX_SCALE, 2, title, theme_titlebar_text(), GFX_SCALE);


/*
    Texte clair force (voir console_set_fg_override()) : sur
    fond noir, le texte suivrait sinon le theme courant --
    illisible (noir sur noir) des que le theme clair est actif.
*/

console_set_bg_override(0x000000, 1);

console_set_fg_override(0xE0E0E0, 1);

}


/*
    A appeler en sortant d'un ecran "console plein ecran" (voir
    desktop_draw_console_titlebar() ci-dessus), pour que le
    bureau et les autres applications retrouvent leurs couleurs
    normales, liees au theme.
*/

static void desktop_reset_console_colors()
{

console_set_bg_override(0, 0);

console_set_fg_override(0, 0);

console_set_top_margin(0);

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
{ "Terminal", DESKTOP_ACTION_TERMINAL, cmd_terminal_app },
{ "Installer sur le disque", DESKTOP_ACTION_INSTALL, cmd_installer_app },
{ "Deconnexion", DESKTOP_ACTION_LOGOUT, (void*)0 },
{ "Redemarrer", DESKTOP_ACTION_REBOOT, (void*)0 },
{ "Eteindre", DESKTOP_ACTION_SHUTDOWN, (void*)0 }
};


/*
    Raccourcis EPINGLES du Dock (voir desktop_draw_taskbar()
    plus bas), a cote de l'icone du Centre d'applications --
    demande explicitement, meme principe que les icones
    epinglees a gauche du separateur dans le Dock macOS. Un
    sous-ensemble volontairement court des entrees ci-dessus
    (les plus utilisees) : le Centre d'applications reste le
    seul endroit qui liste TOUT.
*/

#define DOCK_SHORTCUT_COUNT 3

typedef struct
{

void (*window_entry)();

desktop_action icon_action;

} dock_shortcut;

static const dock_shortcut DOCK_SHORTCUTS[DOCK_SHORTCUT_COUNT] = {
{ cmd_files, DESKTOP_ACTION_FILES },
{ cmd_terminal_app, DESKTOP_ACTION_TERMINAL },
{ cmd_settings, DESKTOP_ACTION_SETTINGS }
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
    Retrouve l'icone d'une fenetre OUVERTE (Dock, voir
    desktop_draw_taskbar()) en retrouvant son entree parmi
    APP_CENTER_ENTRIES par la fonction de lancement -- c'est
    cette fonction, pas le titre affiche, qui identifie
    l'application de facon fiable (deux fenetres pourraient en
    theorie partager un titre). Repli tres rare (fenetre lancee
    autrement que depuis le Centre d'applications, aucun cas
    reel aujourd'hui) : premiere lettre du titre.
*/

static void icon_draw_for_window(const gui_window_slot* w, u32 px, u32 py, u32 size, gfx_color color)
{

for (int i = 0; i < APP_CENTER_ENTRY_COUNT; i++)
{

if (APP_CENTER_ENTRIES[i].window_entry == w->entry)
{

icon_draw_for_action(APP_CENTER_ENTRIES[i].action, px, py, size, color);

return;

}

}


char buf[2];

buf[0] = w->title[0];

buf[1] = 0;

gfx_draw_text(px + size / 4, py + size / 4, buf, color, GFX_SCALE);

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

desktop_draw_console_titlebar("Redemarrage");

console_set_top_margin(1);

power_reboot();

/* Ne revient jamais si power_reboot() reussit -- pas besoin de desktop_reset_console_colors() ici. */

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


desktop_reset_console_colors();

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

case DESKTOP_ACTION_LOGOUT: gui_cursor_erase(); return 1;

case DESKTOP_ACTION_REBOOT: gui_cursor_erase(); desktop_run_reboot(); return 0;

case DESKTOP_ACTION_SHUTDOWN: gui_cursor_erase(); desktop_run_shutdown(); return 0;

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

desktop_hitbox dock_shortcuts[DOCK_SHORTCUT_COUNT];

int app_center_open;

desktop_hitbox app_center_entries[APP_CENTER_ENTRY_COUNT];

desktop_hitbox app_center_panel;

} desktop_state;


/*
    Barre de menu superieure, façon macOS : bande fine et
    quasi-opaque en haut de l'ecran, nom du systeme a gauche,
    horloge ancree a droite (deplacee ici depuis l'ancien
    emplacement bas-droite du Dock, voir desktop_draw_taskbar()
    plus bas -- convention macOS, contrairement a
    Windows/GNOME qui la placent en bas).
*/

#define MENUBAR_H (12 * GFX_SCALE)


static void desktop_draw_menubar()
{


u32 screen_w = gfx_width();


gfx_fill_rect_blend(0, 0, screen_w, MENUBAR_H, theme_titlebar_bg(), 95);

gfx_draw_hline(0, MENUBAR_H - 1, screen_w, theme_border());


u32 text_y = (MENUBAR_H - 8 * GFX_SCALE) / 2;


gfx_draw_text(8 * GFX_SCALE, text_y, "MikeaOS", theme_titlebar_text(), GFX_SCALE);


char clock_text[16];

format_walltime(clock_text);


u32 clock_len = 0;

while (clock_text[clock_len] != 0) { clock_len++; }

u32 clock_px_w = clock_len * 8 * GFX_SCALE;

u32 clock_x = screen_w - clock_px_w - 8 * GFX_SCALE;


gfx_draw_text(clock_x, text_y, clock_text, theme_titlebar_text(), GFX_SCALE);


}


/*
    Dock, façon macOS : barre "verre depoli" a coins arrondis,
    CENTREE horizontalement et flottant a quelques pixels du bas
    de l'ecran (contrairement a l'ancienne barre des taches,
    pleine largeur et ancree a gauche, façon Windows 11/GNOME).
    De gauche a droite : icone du Centre d'applications
    ("Launchpad"), separateur, raccourcis EPINGLES
    (DOCK_SHORTCUTS -- demande explicitement, "a cote" du Centre
    d'applications), puis -- seulement s'il y en a -- un second
    separateur et UNE ICONE PAR FENETRE ACTUELLEMENT OUVERTE
    (avec un point d'accent bleu en dessous des fenetres non
    reduites, meme codage que le petit point macOS sous les
    applications en cours d'execution). Icones seules, sans
    libelle textuel, comme le vrai Dock macOS -- ce qui
    distingue le plus cette barre de l'ancienne liste de boutons
    texte.
*/

#define DOCK_ICON_SIZE (18 * GFX_SCALE)
#define DOCK_GAP (5 * GFX_SCALE)
#define DOCK_PADDING (6 * GFX_SCALE)
#define DOCK_SEP_W (1 * GFX_SCALE)
#define DOCK_SEP_MARGIN (5 * GFX_SCALE)
#define DOCK_BOTTOM_MARGIN (8 * GFX_SCALE)


u32 desktop_dock_top_px()
{

u32 screen_h = gfx_height();

u32 dock_h = DOCK_PADDING * 2 + DOCK_ICON_SIZE;

return screen_h - dock_h - DOCK_BOTTOM_MARGIN;

}


static void desktop_draw_taskbar(desktop_state* state)
{


u32 screen_w = gfx_width();

u32 screen_h = gfx_height();


/* Fenetres actuellement ouvertes (voir gui/window.c) -- determine la largeur du Dock et sa portion droite. */

/*
    Fenetres ouvertes dont l'application EST DEJA epinglee
    (DOCK_SHORTCUTS ci-dessus) sont exclues d'ici : elles
    restent visibles via le point d'accent sous leur icone
    epinglee (voir la boucle DOCK_SHORTCUTS plus haut) --
    exactement comme le vrai Dock macOS, qui n'affiche jamais
    deux fois la meme application. Sans ce filtre, ouvrir le
    Terminal (epingle par defaut) faisait apparaitre deux
    icones Terminal cote a cote.
*/

int open_slots[GUI_MAX_WINDOWS];

int open_count = 0;

for (int i = 0; i < GUI_MAX_WINDOWS; i++)
{

const gui_window_slot* w = gui_window_get(i);

if (w == 0)
{

continue;

}


int is_pinned = 0;

for (int p = 0; p < DOCK_SHORTCUT_COUNT; p++)
{

if (DOCK_SHORTCUTS[p].window_entry == w->entry)
{

is_pinned = 1;

break;

}

}


if (is_pinned)
{

continue;

}


open_slots[open_count] = i;

open_count++;

}


int total_icons = 1 + DOCK_SHORTCUT_COUNT + open_count;

u32 icons_w = (u32)total_icons * DOCK_ICON_SIZE + (u32)(total_icons - 1) * DOCK_GAP;

u32 sep_w = DOCK_SEP_W + DOCK_SEP_MARGIN * 2;

u32 seps_w = (open_count > 0) ? sep_w * 2 : sep_w;


u32 dock_w = DOCK_PADDING * 2 + icons_w + seps_w;

u32 dock_h = DOCK_PADDING * 2 + DOCK_ICON_SIZE;


u32 dock_x = (screen_w > dock_w) ? (screen_w - dock_w) / 2 : 0;

u32 dock_y = screen_h - dock_h - DOCK_BOTTOM_MARGIN;


gfx_fill_rect_blend(dock_x + 3, dock_y + 3, dock_w, dock_h, theme_shadow(), 18);

gfx_fill_rounded_rect_blend(dock_x, dock_y, dock_w, dock_h, DOCK_ICON_SIZE / 2, theme_titlebar_bg(), 80);


u32 icon_y = dock_y + DOCK_PADDING;

u32 cursor_x = dock_x + DOCK_PADDING;


/* Icone "Launchpad" (Centre d'applications) : grille 3x3, meme motif que le bouton "Toutes les applications" d'origine. */

icon_draw_grid(cursor_x, icon_y, DOCK_ICON_SIZE, theme_accent());

state->apps_button.x = cursor_x;

state->apps_button.y = dock_y;

state->apps_button.w = DOCK_ICON_SIZE;

state->apps_button.h = dock_h;

cursor_x += DOCK_ICON_SIZE + DOCK_GAP;


/* Separateur avant les raccourcis epingles. */

cursor_x += DOCK_SEP_MARGIN;

gfx_draw_vline(cursor_x, icon_y, DOCK_ICON_SIZE, theme_border());

cursor_x += DOCK_SEP_W + DOCK_SEP_MARGIN;


/* Raccourcis epingles (voir DOCK_SHORTCUTS plus haut). */

for (int i = 0; i < DOCK_SHORTCUT_COUNT; i++)
{

icon_draw_for_action(DOCK_SHORTCUTS[i].icon_action, cursor_x, icon_y, DOCK_ICON_SIZE, theme_text());


if (gui_window_find_by_entry(DOCK_SHORTCUTS[i].window_entry) >= 0)
{

gfx_fill_circle(cursor_x + DOCK_ICON_SIZE / 2, dock_y + dock_h - (u32)(2 * GFX_SCALE), (u32)GFX_SCALE, theme_accent());

}


state->dock_shortcuts[i].x = cursor_x;

state->dock_shortcuts[i].y = dock_y;

state->dock_shortcuts[i].w = DOCK_ICON_SIZE;

state->dock_shortcuts[i].h = dock_h;


cursor_x += DOCK_ICON_SIZE + DOCK_GAP;

}


/* Une icone par fenetre ouverte -- seulement si au moins une l'est (sinon, pas de second separateur "pour rien"). */

for (int i = 0; i < GUI_MAX_WINDOWS; i++)
{

state->window_buttons[i].w = 0;

}


if (open_count > 0)
{


cursor_x += DOCK_SEP_MARGIN;

gfx_draw_vline(cursor_x, icon_y, DOCK_ICON_SIZE, theme_border());

cursor_x += DOCK_SEP_W + DOCK_SEP_MARGIN;


for (int k = 0; k < open_count; k++)
{

int slot = open_slots[k];

const gui_window_slot* w = gui_window_get(slot);


icon_draw_for_window(w, cursor_x, icon_y, DOCK_ICON_SIZE, theme_text());


if (!w->minimized)
{

gfx_fill_circle(cursor_x + DOCK_ICON_SIZE / 2, dock_y + dock_h - (u32)(2 * GFX_SCALE), (u32)GFX_SCALE, theme_accent());

}


state->window_buttons[slot].x = cursor_x;

state->window_buttons[slot].y = dock_y;

state->window_buttons[slot].w = DOCK_ICON_SIZE;

state->window_buttons[slot].h = dock_h;


cursor_x += DOCK_ICON_SIZE + DOCK_GAP;

}


}


desktop_draw_menubar();


}


#define LAUNCHPAD_COLS 5

#define LAUNCHPAD_CELL_W (14 * 8)

#define LAUNCHPAD_CELL_H (13 * 8)

#define LAUNCHPAD_ICON_SIZE (14 * GFX_SCALE)

/*
    Correctif (chevauchement des libelles) : les libellés
    (jusqu'a "Installer sur le disque", 24 caracteres) etaient
    dessines a l'echelle GFX_SCALE (2, soit 16px/caractere) dans
    des cellules bien trop etroites pour ca -- le texte de
    chaque cellule debordait largement sur ses voisines,
    illisible. Deux corrections : une echelle de texte plus
    petite specifique aux libelles (1, soit 8px/caractere), et
    une troncature (avec "..") de tout libelle qui ne tiendrait
    toujours pas dans la largeur d'une cellule.
*/

#define LAUNCHPAD_LABEL_SCALE 1

#define LAUNCHPAD_LABEL_MAX_CHARS ((LAUNCHPAD_CELL_W - 4) / (8 * LAUNCHPAD_LABEL_SCALE))


/*
    Copie "label" dans "out" (taille "out_size"), tronque a
    LAUNCHPAD_LABEL_MAX_CHARS caracteres avec un "..." final si
    besoin -- voir le commentaire de LAUNCHPAD_LABEL_SCALE
    ci-dessus.
*/

static void launchpad_truncate_label(const char* label, char* out, u32 out_size)
{

u32 len = 0;

while (label[len] != 0) { len++; }


if (len <= LAUNCHPAD_LABEL_MAX_CHARS || out_size < 4)
{

u32 i = 0;

while (label[i] != 0 && i < out_size - 1) { out[i] = label[i]; i++; }

out[i] = 0;

return;

}


u32 keep = LAUNCHPAD_LABEL_MAX_CHARS - 2;

if (keep > out_size - 4) { keep = out_size - 4; }


u32 i = 0;

while (i < keep) { out[i] = label[i]; i++; }

out[i] = '.'; out[i+1] = '.'; out[i+2] = 0;

}


/*
    Centre d'applications ("Launchpad") : grille d'icones
    centree sur un fond assombri qui couvre tout l'ecran --
    meme principe visuel que le Launchpad macOS, tres different
    du panneau-liste vertical d'origine (inspire du menu
    Demarrer Windows 11/tiroir GNOME). Icone + libelle par
    cellule, sans bordure de panneau individuelle (juste un
    halo "verre depoli" discret derriere chaque icone).

    Texte/icones toujours en BLANC ici, quel que soit le theme
    clair/sombre courant (voir gui/theme.c) : contrairement au
    reste de l'interface, le fond de cette grille est TOUJOURS
    sombre (l'assombrissement ci-dessous), donc theme_text()
    (quasi noir en theme clair) y serait illisible.
*/

static void desktop_draw_app_center(desktop_state* state)
{


u32 screen_w = gfx_width();

u32 screen_h = gfx_height();


gfx_color launchpad_text = 0xF2F2F7;


/* Assombrit tout l'ecran -- meme effet que l'ouverture du Launchpad sur macOS. */

gfx_fill_rect_blend(0, 0, screen_w, screen_h, 0x000000, 45);


int rows_needed = (APP_CENTER_ENTRY_COUNT + LAUNCHPAD_COLS - 1) / LAUNCHPAD_COLS;


u32 grid_w = (u32)LAUNCHPAD_COLS * LAUNCHPAD_CELL_W;

u32 grid_h = (u32)rows_needed * LAUNCHPAD_CELL_H;


u32 grid_x = (screen_w > grid_w) ? (screen_w - grid_w) / 2 : 0;

u32 grid_y = (screen_h > grid_h) ? (screen_h - grid_h) / 2 : 0;


state->app_center_panel.x = grid_x;

state->app_center_panel.y = grid_y;

state->app_center_panel.w = grid_w;

state->app_center_panel.h = grid_h;


for (int i = 0; i < APP_CENTER_ENTRY_COUNT; i++)
{


int col = i % LAUNCHPAD_COLS;

int row = i / LAUNCHPAD_COLS;


u32 cell_x = grid_x + (u32)col * LAUNCHPAD_CELL_W;

u32 cell_y = grid_y + (u32)row * LAUNCHPAD_CELL_H;


u32 halo_margin = 3 * GFX_SCALE;

u32 halo_w = LAUNCHPAD_CELL_W - halo_margin * 2;

u32 halo_h = LAUNCHPAD_ICON_SIZE + halo_margin * 2;


gfx_fill_rounded_rect_blend(cell_x + halo_margin, cell_y + halo_margin, halo_w, halo_h, halo_margin * 2, 0xFFFFFF, 12);


u32 icon_x = cell_x + (LAUNCHPAD_CELL_W - LAUNCHPAD_ICON_SIZE) / 2;

u32 icon_y = cell_y + halo_margin * 2;


icon_draw_for_action(APP_CENTER_ENTRIES[i].action, icon_x, icon_y, LAUNCHPAD_ICON_SIZE, launchpad_text);


char label_buf[32];

launchpad_truncate_label(APP_CENTER_ENTRIES[i].label, label_buf, sizeof(label_buf));


u32 label_len = 0;

while (label_buf[label_len] != 0) { label_len++; }

u32 label_px_w = label_len * 8 * LAUNCHPAD_LABEL_SCALE;

u32 label_x = (LAUNCHPAD_CELL_W > label_px_w) ? cell_x + (LAUNCHPAD_CELL_W - label_px_w) / 2 : cell_x;

u32 label_y = icon_y + LAUNCHPAD_ICON_SIZE + 4 * GFX_SCALE;


gfx_draw_text(label_x, label_y, label_buf, launchpad_text, LAUNCHPAD_LABEL_SCALE);


state->app_center_entries[i].x = cell_x;

state->app_center_entries[i].y = cell_y;

state->app_center_entries[i].w = LAUNCHPAD_CELL_W;

state->app_center_entries[i].h = LAUNCHPAD_CELL_H;


}


}


/*
    "Papier peint" procedural : les 6 motifs geometriques
    d'origine (voir gui/theme.h, wallpaper_style), toujours
    disponibles -- utilises quand aucune photo n'est
    selectionnee (voir wallpaper_get_photo_index(),
    gui_paint_photo_area() juste en dessous pour les VRAIES
    photos). Dessines dans un rectangle arbitraire ("x","y","w",
    "h") plutot que forcement plein ecran : reutilisee telle
    quelle pour les vignettes miniatures du selecteur de
    Parametres.
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


/*
    Vraie photo (voir gui/assets/wallpaper_images.h), affichee
    avec un LISSAGE BILINEAIRE plutot qu'en blocs bruts : pour
    chaque pixel ecran, on interpole entre les 4 pixels source
    les plus proches au lieu d'etaler chaque pixel source en un
    seul bloc uni -- nettement moins "pixelise" a l'oeil pour
    la meme resolution embarquee (voir wallpaper_images.h pour
    les contraintes de taille qui la limitent). Arithmetique en
    virgule fixe (8 bits de fraction) : ce noyau est compile
    sans support flottant (-mno-sse/-mno-sse2, voir gui/icons.c
    pour la meme contrainte deja rencontree).
*/

void gui_paint_photo_area(u32 x, u32 y, u32 w, u32 h, u32 photo_index)
{

if (photo_index >= wallpaper_photo_count() || w == 0 || h == 0)
{

return;

}


const wallpaper_photo* photo = &WALLPAPER_PHOTOS[photo_index];


if (photo->width < 2 || photo->height < 2)
{

return;

}


for (u32 py = 0; py < h; py++)
{


/*
    Position source (en virgule fixe, 8 bits de fraction) :
    fait correspondre le pixel ecran "py" a une position
    FRACTIONNAIRE dans la photo, entre 0 et (height-1).
*/

u32 sy_fixed = (h > 1) ? (py * (photo->height - 1) * 256) / (h - 1) : 0;

u32 y0 = sy_fixed >> 8;

u32 frac_y = sy_fixed & 0xFF;

u32 y1 = (y0 + 1 < photo->height) ? y0 + 1 : y0;


for (u32 px = 0; px < w; px++)
{


u32 sx_fixed = (w > 1) ? (px * (photo->width - 1) * 256) / (w - 1) : 0;

u32 x0 = sx_fixed >> 8;

u32 frac_x = sx_fixed & 0xFF;

u32 x1 = (x0 + 1 < photo->width) ? x0 + 1 : x0;


const u8* p00 = &photo->rgb_data[(y0 * photo->width + x0) * 3];

const u8* p10 = &photo->rgb_data[(y0 * photo->width + x1) * 3];

const u8* p01 = &photo->rgb_data[(y1 * photo->width + x0) * 3];

const u8* p11 = &photo->rgb_data[(y1 * photo->width + x1) * 3];


u8 out_rgb[3];


for (int c = 0; c < 3; c++)
{

u32 top = (u32)p00[c] * (256 - frac_x) + (u32)p10[c] * frac_x;

u32 bottom = (u32)p01[c] * (256 - frac_x) + (u32)p11[c] * frac_x;

u32 blended = (top * (256 - frac_y) + bottom * frac_y) >> 16;

out_rgb[c] = (u8)blended;

}


gfx_color pixel_color = ((gfx_color)out_rgb[0] << 16) | ((gfx_color)out_rgb[1] << 8) | (gfx_color)out_rgb[2];


gfx_put_pixel(x + px, y + py, pixel_color);


}


}


}


static void desktop_paint_wallpaper()
{

int photo_index = wallpaper_get_photo_index();


if (photo_index >= 0)
{

gui_paint_photo_area(0, 0, gfx_width(), gfx_height(), (u32)photo_index);

return;

}


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


/*
    Renvoie l'indice du raccourci epingle du Dock (DOCK_SHORTCUTS)
    sous le point (mx,my), ou -1 si aucun -- utilise par
    gui_desktop_run() pour distinguer un clic sur un raccourci
    d'un clic sur l'icone du Centre d'applications ou sur une
    fenetre ouverte (toutes des zones voisines dans le Dock).
*/

static int desktop_hit_dock_shortcut(desktop_state* state, s32 mx, s32 my)
{

for (int i = 0; i < DOCK_SHORTCUT_COUNT; i++)
{

if (gui_point_in_rect(
state->dock_shortcuts[i].x, state->dock_shortcuts[i].y,
state->dock_shortcuts[i].w, state->dock_shortcuts[i].h,
mx, my
))
{

return i;

}

}

return -1;

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
    Correctif (cliquer deux fois sur la meme application
    ouvrait une deuxieme fenetre au lieu de revenir sur la
    premiere) : si une fenetre de ce type existe deja
    (reduite ou non), on lui redonne simplement le focus au
    lieu d'en lancer une autre -- comportement attendu d'un
    lanceur d'applications habituel (Windows, macOS, GNOME).
*/

int existing = gui_window_find_by_entry(APP_CENTER_ENTRIES[matched].window_entry);


if (existing >= 0)
{

gui_window_focus(existing);

}
else
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

else if (
(
desktop_hit_dock_shortcut(&state, mx, my) >= 0
)
)
{


int matched_shortcut = desktop_hit_dock_shortcut(&state, mx, my);


/*
    Meme logique que le Centre d'applications : redonne le
    focus a une fenetre deja ouverte de ce type au lieu d'en
    ouvrir une seconde.
*/

int existing = gui_window_find_by_entry(DOCK_SHORTCUTS[matched_shortcut].window_entry);

if (existing >= 0)
{

gui_window_focus(existing);

}
else
{


/* Retrouve le libelle correspondant parmi APP_CENTER_ENTRIES, pour un titre de fenetre correct (voir gui/window.h, gui_window_slot.title). */

const char* title = "Application";

for (int i = 0; i < APP_CENTER_ENTRY_COUNT; i++)
{

if (APP_CENTER_ENTRIES[i].window_entry == DOCK_SHORTCUTS[matched_shortcut].window_entry)
{

title = APP_CENTER_ENTRIES[i].label;

break;

}

}


int opened = gui_window_open(title, DOCK_SHORTCUTS[matched_shortcut].window_entry);

if (opened < 0)
{

sound_play_error();

}

}

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
    Raccourci clavier : Entree ouvre directement le Terminal
    (ou lui redonne le focus s'il est deja ouvert -- meme
    logique que le Centre d'applications), pour qui prefere le
    clavier a la souris.
*/

if (!state.app_center_open && keyboard_available())
{

char c = keyboard_getchar();

if (c == '\n')
{

int existing = gui_window_find_by_entry(cmd_terminal_app);

if (existing >= 0)
{

gui_window_focus(existing);

}
else
{

gui_window_open("Terminal", cmd_terminal_app);

}

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
