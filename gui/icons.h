#ifndef MIKEA_GUI_ICONS_H
#define MIKEA_GUI_ICONS_H


#include "../include/types.h"

#include "../kernel/drivers/graphics/graphics.h"


/*
    ============================================================
    Mikea OS - Icones
    ============================================================

    Avant ce fichier, chaque bouton de la barre des taches
    n'etait qu'un libelle texte ("Fichiers", "Calc"...) --
    fonctionnel, mais tres eloigne de l'apparence attendue d'un
    bureau moderne (Windows/GNOME/macOS affichent tous une
    icone reconnaissable avant meme de lire le texte). Ce
    fichier fournit des icones simples, dessinees en pixels a
    partir des primitives de kernel/drivers/graphics (aucun
    decodeur d'image ici : ni PNG, ni BMP, seulement des formes
    geometriques), monochromes (une seule couleur, fournie par
    l'appelant -- voir gui/theme.c) pour s'adapter automatiquement
    au theme clair/sombre courant sans avoir besoin de deux jeux
    d'images.

    Chaque icone se dessine dans un carre de cote "size" pixels,
    coin superieur gauche (px, py). Un "size" d'au moins 16
    pixels est recommande pour rester lisible (en dessous, les
    proportions internes -- ex. la grille de touches de la
    calculatrice -- s'ecrasent).
    ============================================================
*/


/* Dossier (icone "Fichiers" / explorateur). */

void icon_draw_folder(u32 px, u32 py, u32 size, gfx_color color);


/* Calculatrice : cadre, ecran, grille de touches. */

void icon_draw_calculator(u32 px, u32 py, u32 size, gfx_color color);


/* Terminal : cadre d'ecran avec l'invite ">_ ". */

void icon_draw_terminal(u32 px, u32 py, u32 size, gfx_color color);


/*
    Curseurs de reglage (icone "Parametres") : trois lignes
    horizontales avec une poignee chacune, motif tres repandu
    pour "reglages"/"preferences".
*/

void icon_draw_settings(u32 px, u32 py, u32 size, gfx_color color);


/* Silhouette tete + epaules (icone "Compte"). */

void icon_draw_user(u32 px, u32 py, u32 size, gfx_color color);


/* Symbole d'alimentation (anneau + trait) : icone "Deconnexion". */

void icon_draw_power(u32 px, u32 py, u32 size, gfx_color color);


/*
    Grille 3x3 de petits carres (icone du lanceur "Toutes les
    applications", voir gui/desktop.c) -- meme motif que le
    bouton "Toutes les applications" de Windows 11 ou le tiroir
    d'applications d'Android/GNOME.
*/

void icon_draw_grid(u32 px, u32 py, u32 size, gfx_color color);


#endif
