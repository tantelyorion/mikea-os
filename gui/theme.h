#ifndef MIKEA_GUI_THEME_H
#define MIKEA_GUI_THEME_H


#include "../include/types.h"

#include "../kernel/drivers/graphics/graphics.h"


/*
    ============================================================
    Mikea OS - Theme
    ============================================================

    Remplace l'ancien theme "futuriste" (fond bleu nuit, accents
    cyan neon, coins en "L" style HUD de vaisseau) par un theme
    neutre noir/blanc/gris, proche de ce qu'on trouve sur macOS,
    GNOME, Windows ou Openbox : fond clair (ou sombre), panneaux
    "verre depoli" (glassmorphisme, voir gfx_fill_rect_blend()
    dans kernel/drivers/graphics), bordures fines discretes,
    ombre portee douce plutot que neon.

    Un seul etat global (clair/sombre) suffit ici : ce systeme
    n'a qu'un seul framebuffer et pas de notion de "fenetre
    active vs inactive" a themer separement pour l'instant.
    theme_toggle_dark_mode() (voir apps/settings) permet de
    basculer a l'execution.
    ============================================================
*/


void theme_set_dark(int enabled);

void theme_toggle_dark_mode();

int theme_is_dark();


/* Couleur de fond du bureau (derriere les fenetres). */

gfx_color theme_desktop_bg();


/*
    Couleur "de base" du panneau d'une fenetre, avant effet de
    transparence -- voir gfx_fill_rect_blend(). Ce n'est pas la
    couleur finale a l'ecran : le fond existant transparait au
    travers (effet verre depoli).
*/

gfx_color theme_panel();


/* Opacite du panneau "verre depoli", en pourcentage (0-100). */

u32 theme_panel_opacity();


/* Bordure fine autour des fenetres/boutons. */

gfx_color theme_border();


/* Fond de la barre de titre (plus opaque que le corps de la fenetre). */

gfx_color theme_titlebar_bg();


/* Couleur du texte (titre, contenu). */

gfx_color theme_text();

gfx_color theme_titlebar_text();


/* Fond / bordure des boutons standards. */

gfx_color theme_button_bg();

gfx_color theme_button_border();


/* Bouton de fermeture ("point rouge" façon macOS). */

gfx_color theme_close_bg();

gfx_color theme_close_symbol();


/* Couleur du curseur souris. */

gfx_color theme_cursor();

gfx_color theme_cursor_outline();


/*
    Couleur d'ombre portee douce (utilisee avec une faible
    opacite par gfx_fill_rect_blend(), jamais dessinee pleine).
*/

gfx_color theme_shadow();


/*
    Attribut de couleur VGA (mode texte, palette 16 couleurs
    fixe -- pas de glassmorphisme possible ici, mais on garde la
    meme logique noir/blanc/gris que le mode graphique) : texte
    clair sur fond sombre en theme sombre, texte sombre sur fond
    gris clair en theme clair.
*/

u8 theme_text_attr();


#endif
