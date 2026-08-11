#ifndef MIKEA_GUI_H
#define MIKEA_GUI_H


#include "types.h"


/*
    ============================================================
    Mikea OS - GUI (mode texte)
    ============================================================

    IMPORTANT :
    Le pilote video actuel (kernel/drivers/framebuffer.c)
    pilote la VGA en MODE TEXTE 80x25 (voir aussi la remarque
    dans assets/wallpapers/wallpaper.h). Il n'existe PAS de
    pilote graphique en pixels (VESA/VBE ou GOP) dans ce
    projet pour l'instant.

    Ce module fournit donc une interface graphique reelle mais
    limitee a des caracteres et a des couleurs VGA (fenetres
    dessinees avec des bordures ASCII, pas de pixels ni
    d'images). Les polices/icones/fonds d'ecran de assets/
    restent des reserves pour le jour ou un vrai framebuffer
    en pixels existera : ils ne sont pas utilises ici.
    ============================================================
*/


/*
    Dessine une boite avec bordure a la position (x, y),
    de largeur "width" et hauteur "height" (en caracteres).
*/

void gui_draw_box(int x, int y, int width, int height, u8 color);


/*
    Dessine une "fenetre" : une boite avec une barre de
    titre inversee (fond/texte inverses) sur sa premiere
    ligne.
*/

void gui_draw_window(int x, int y, int width, int height, const char* title, u8 color);


/*
    Ecrit du texte a une position donnee, sans dependre du
    curseur "fil de l'eau" de kernel/console (utile a
    l'interieur d'une fenetre).
*/

void gui_draw_text(int x, int y, const char* text, u8 color);


#endif
