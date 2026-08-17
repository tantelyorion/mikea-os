#ifndef MIKEA_GUI_H
#define MIKEA_GUI_H


#include "../include/types.h"


/*
    ============================================================
    Mikea OS - GUI
    ============================================================

    Bascule automatiquement entre deux rendus :
    - Mode texte (defaut historique) : caracteres/couleurs VGA
      via kernel/drivers/framebuffer.c.
    - Mode graphique pixels (des que gfx_available() est vrai,
      voir kernel/drivers/graphics/) : fenetre reelle avec
      bordure, fond distinct et police bitmap agrandie.

    Etape 5 (interaction souris <-> fenetre) : bouton de
    fermeture cliquable et curseur souris, mode graphique
    uniquement -- sans effet en mode texte (pas de souris a
    afficher dans un terminal texte classique).
    ============================================================
*/


/*
    Dessine une boite avec bordure a la position (x, y),
    de largeur "width" et hauteur "height" (en caracteres).
*/

void gui_draw_box(int x, int y, int width, int height, u8 color);


/*
    Dessine une "fenetre" : une boite avec une barre de
    titre sur sa premiere ligne. En mode graphique, dessine
    aussi un bouton de fermeture ('X') dans le coin superieur
    droit de la barre de titre, et renseigne ses coordonnees
    pixel (bx, by, bsize) pour un test de collision avec la
    souris -- voir gui_point_in_button(). Les pointeurs de
    sortie peuvent etre nuls si l'appelant n'en a pas besoin
    (ex. mode texte).
*/

void gui_draw_window(int x, int y, int width, int height, const char* title, u8 color, u32* bx, u32* by, u32* bsize);


/*
    Ecrit du texte a une position donnee, sans dependre du
    curseur "fil de l'eau" de kernel/console (utile a
    l'interieur d'une fenetre).
*/

void gui_draw_text(int x, int y, const char* text, u8 color);


/*
    Dessine un curseur souris simple a la position pixel
    donnee. Sans effet en mode texte.
*/

void gui_draw_cursor(s32 x, s32 y);


/*
    Teste si le point pixel (px, py) se trouve dans un bouton
    carre de cote "bsize" dont le coin superieur gauche est
    (bx, by) -- voir les coordonnees renvoyees par
    gui_draw_window().
*/

int gui_point_in_button(u32 bx, u32 by, u32 bsize, s32 px, s32 py);


#endif
