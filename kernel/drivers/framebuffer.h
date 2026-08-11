#ifndef MIKEA_FRAMEBUFFER_H
#define MIKEA_FRAMEBUFFER_H


#include "../../include/types.h"


/*
    Pilote video VGA en MODE TEXTE (buffer 0xB8000,
    grille de caracteres 80x25). Avant ce fichier, ces
    fonctions n'avaient pas de header et etaient
    redeclarees a la main (et de facon incomplete) dans
    chaque fichier qui les utilisait.
*/


/*
    Dimensions de la grille VGA texte. Exposees ici (au lieu
    de rester des macros privees dans framebuffer.c) pour que
    console.c puisse detecter correctement les depassements
    de ligne/colonne au lieu de les deviner.
*/

#define FB_WIDTH  80

#define FB_HEIGHT 25


void fb_clear();

void fb_put(int x, int y, char c, u8 color);

void fb_write(const char* text, int x, int y);


/*
    Fait defiler l'ecran d'une ligne vers le haut (la ligne 0
    est perdue, chaque ligne prend la place de la precedente)
    et vide la derniere ligne. A utiliser quand le curseur
    atteint FB_HEIGHT.
*/

void fb_scroll();


#endif
