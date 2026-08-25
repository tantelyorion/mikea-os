#ifndef MIKEA_CONSOLE_H
#define MIKEA_CONSOLE_H


#include "../drivers/graphics/graphics.h"


void console_init();

void console_write(
const char* text
);


/*
    Efface visuellement le dernier caractere affiche (recule
    le curseur d'une position, y compris en debut de ligne,
    puis ecrit un espace a cet endroit). Utilisee pour l'echo
    de la touche Retour arriere par input_readline().
*/

void console_backspace();


/*
    Efface tout l'ecran (via fb_clear()) ET remet le curseur
    en haut a gauche (0,0).

    Correctif : la commande shell "clear" appelait directement
    fb_clear() (kernel/drivers/framebuffer.c), qui ne vide que
    la memoire video -- elle ne sait rien du curseur "fil de
    l'eau" gere ici par cursor_x/cursor_y. Sans remise a zero,
    le texte suivant continuait a s'ecrire a l'ancienne
    position du curseur (ex. au milieu de l'ecran) au lieu de
    reprendre en haut d'un ecran desormais vide.
*/

void console_clear();


/*
    Reserve "rows" lignes de texte en haut de l'ecran, jamais
    ecrites ni effacees par console_write()/console_clear() --
    utilisee par les applications qui dessinent leur propre
    barre de titre par-dessus une console plein ecran (voir
    gui/desktop.c, desktop_run_terminal() et
    desktop_run_installer()) : sans ca, le texte du terminal
    remontait par-dessus la barre de titre des le premier
    defilement. "rows" a 0 desactive la marge (comportement
    d'origine, plein ecran des la ligne 0).
*/

void console_set_top_margin(int rows);


/*
    Force temporairement la couleur de texte/fond de la console
    graphique, quel que soit le theme courant (voir
    gui/theme.c) -- utilisee pour donner au Terminal/
    Installateur (gui/desktop.c) leur fond noir traditionnel,
    independant du theme clair/sombre choisi dans Parametres.
    "enabled" a 0 desactive la surcharge (retour au theme
    normal, comportement par defaut).
*/

void console_set_fg_override(gfx_color color, int enabled);

void console_set_bg_override(gfx_color color, int enabled);


/*
    Capture tout ce que console_write() ecrirait normalement a
    l'ecran dans "buffer" (jusqu'a "capacity" octets, toujours
    termine par un caractere nul) au lieu de l'afficher -- voir
    le commentaire dans console.c. console_redirect_stop()
    retablit l'affichage normal.
*/

void console_redirect_start(char* buffer, u32 capacity);

void console_redirect_stop();


#endif