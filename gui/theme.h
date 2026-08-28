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


/*
    Feux tricolores macOS des deux autres boutons de barre de
    titre (voir gui_draw_window(), gui/gui.c) -- jaune pour
    reduire, vert pour agrandir, meme codage que le vrai macOS.
    Seul theme_close_bg() (rouge) existait avant ce correctif :
    c'etait le "seul accent de couleur du theme" mentionne plus
    haut dans ce fichier -- desormais complete par l'accent bleu
    ci-dessous et ces deux couleurs, demande explicitement pour
    se rapprocher visuellement de macOS.
*/

gfx_color theme_minimize_bg();

gfx_color theme_maximize_bg();


/*
    Accent bleu (façon macOS -- barre des taches/Dock active,
    icone du Centre d'applications, elements selectionnes des
    Parametres). Vient s'ajouter a la palette noir/blanc/gris
    existante SANS la remplacer (voir le commentaire en tete de
    ce fichier) : un seul point d'accent de plus, comme le rouge
    du bouton de fermeture, pas un theme bleu general.
*/

gfx_color theme_accent();

/* Texte/icone lisible PAR-DESSUS theme_accent() (blanc dans les deux modes -- le bleu choisi reste assez fonce pour ca). */

gfx_color theme_accent_text();


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


/*
    ============================================================
    Fond d'ecran (voir gui/desktop.c, desktop_paint_wallpaper())
    ============================================================

    Limite assumee, honnetement documentee plutot que masquee :
    ce noyau n'a AUCUN decodeur d'image (ni JPEG, ni PNG, ni
    BMP -- en ecrire un est un chantier a part entiere). Il est
    donc impossible d'afficher une vraie photo comme fond
    d'ecran. A la place, 6 MOTIFS PROCEDURAUX distincts,
    dessines avec les memes primitives que le reste de
    l'interface (kernel/drivers/graphics), en niveaux de gris
    coherents avec le theme clair/sombre courant -- un choix
    honnete plutot qu'une fausse promesse de "photos".
*/

typedef enum
{

WALLPAPER_GRADIENT,   /* dsegrade vertical doux (motif d'origine, garde comme reglage par defaut) */

WALLPAPER_STRIPES,    /* fines rayures horizontales alternees */

WALLPAPER_CHECKER,    /* damier a grandes cases */

WALLPAPER_DOTS,       /* grille de points espaces */

WALLPAPER_DIAGONAL,   /* rayures diagonales "en escalier" */

WALLPAPER_SOLID       /* aplat uni (le plus sobre des 6) */

} wallpaper_style;


#define WALLPAPER_STYLE_COUNT 6


/*
    Au-dela des 6 motifs procéduraux ci-dessus : de vraies
    photos (voir gui/assets/wallpaper_images.h), selectionnees
    par leur INDICE dans WALLPAPER_PHOTOS[] plutot que par un
    style parmi wallpaper_style. -1 = aucune photo choisie,
    utiliser un motif procédural a la place (wallpaper_get_style()
    fait toujours foi dans ce cas).
*/

void wallpaper_set_photo_index(int index);

int wallpaper_get_photo_index();


void wallpaper_set_style(wallpaper_style style);

wallpaper_style wallpaper_get_style();


/* Nom court affiche dans le selecteur de apps/settings/settings.c. */

const char* wallpaper_style_name(wallpaper_style style);


#endif
