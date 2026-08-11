#ifndef MIKEA_ICONS_H
#define MIKEA_ICONS_H


#include "../../include/types.h"


/*
    Icones monochromes 8x8 pixels (1 bit par pixel, un octet
    par ligne, bit 7 = pixel de gauche). Simples formes
    generiques dessinees a la main en attendant un vrai
    pilote graphique en pixels (voir assets/wallpapers pour
    la meme remarque).
*/


/* Icone "dossier" */

static const u8 ICON_FOLDER[8] =
{

0b00000000,
0b01111000,
0b11111111,
0b10000001,
0b10000001,
0b10000001,
0b11111111,
0b00000000

};


/* Icone "fichier" */

static const u8 ICON_FILE[8] =
{

0b00000000,
0b01111100,
0b01000100,
0b01000100,
0b01000100,
0b01000100,
0b01111100,
0b00000000

};


/* Icone "terminal / console" */

static const u8 ICON_TERMINAL[8] =
{

0b00000000,
0b11111111,
0b10100001,
0b10010001,
0b10001001,
0b10000001,
0b11111111,
0b00000000

};


#endif
