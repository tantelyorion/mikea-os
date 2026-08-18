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
    Restaure le fond sous la derniere position dessinee du
    curseur, sans rien dessiner de nouveau. A appeler avant de
    quitter une boucle interactive utilisant gui_draw_cursor(),
    pour ne pas laisser de trace visible.
*/

void gui_cursor_erase();


/*
    Teste si le point pixel (px, py) se trouve dans un bouton
    carre de cote "bsize" dont le coin superieur gauche est
    (bx, by) -- voir les coordonnees renvoyees par
    gui_draw_window().
*/

int gui_point_in_button(u32 bx, u32 by, u32 bsize, s32 px, s32 py);


/*
    Version generique (rectangle, pas seulement carre) du test
    de collision ci-dessus -- utilisee par les boutons
    d'application (ex. calculatrice) qui ne sont pas forcement
    carres.
*/

int gui_point_in_rect(u32 rx, u32 ry, u32 rw, u32 rh, s32 px, s32 py);


/*
    Dessine un bouton rectangulaire generique (bordure +
    libelle centre approximativement), en caracteres (x, y,
    width, height). Renvoie ses coordonnees pixel via
    out_x/out_y/out_w/out_h (peuvent etre nuls), pour un test
    de collision avec gui_point_in_rect(). Sans effet en mode
    texte (les applications interactives -- calculatrice etc. --
    necessitent une souris, donc le mode graphique).
*/

void gui_draw_button(int x, int y, int w, int h, const char* label, u32* out_x, u32* out_y, u32* out_w, u32* out_h);


#endif
