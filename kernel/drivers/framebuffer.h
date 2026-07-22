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

void fb_clear();

void fb_put(int x, int y, char c, u8 color);

void fb_write(const char* text, int x, int y);


#endif
