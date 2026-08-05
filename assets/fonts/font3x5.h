#ifndef MIKEA_FONT3X5_H
#define MIKEA_FONT3X5_H


#include "../../include/types.h"


/*
    Police pixel minimaliste 3x5 pour les chiffres 0-9
    (utile pour un futur affichage graphique : horloge,
    compteurs, jauges...). Un octet par ligne, 5 lignes par
    chiffre ; seuls les 3 bits de poids faible sont utilises
    (bit 2 = colonne de gauche, bit 0 = colonne de droite).

    Comme pour les autres fichiers de assets/, ceci ne sera
    reellement affichable qu'une fois un pilote graphique en
    pixels disponible (le pilote actuel est en mode texte
    VGA). En attendant, cette police peut deja servir de base
    pour prototyper un futur rendu.
*/


static const u8 FONT_DIGITS[10][5] =
{

/* 0 */ { 0b111, 0b101, 0b101, 0b101, 0b111 },

/* 1 */ { 0b010, 0b110, 0b010, 0b010, 0b111 },

/* 2 */ { 0b111, 0b001, 0b111, 0b100, 0b111 },

/* 3 */ { 0b111, 0b001, 0b111, 0b001, 0b111 },

/* 4 */ { 0b101, 0b101, 0b111, 0b001, 0b001 },

/* 5 */ { 0b111, 0b100, 0b111, 0b001, 0b111 },

/* 6 */ { 0b111, 0b100, 0b111, 0b101, 0b111 },

/* 7 */ { 0b111, 0b001, 0b001, 0b001, 0b001 },

/* 8 */ { 0b111, 0b101, 0b111, 0b101, 0b111 },

/* 9 */ { 0b111, 0b101, 0b111, 0b001, 0b111 }

};


#endif
