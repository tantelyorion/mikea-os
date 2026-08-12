#ifndef MIKEA_GRAPHICS_H
#define MIKEA_GRAPHICS_H

#include "../../../include/types.h"


/*
    Pilote graphique en pixels (VESA/VBE), etape 2 : lit les
    caracteristiques du framebuffer lineaire deposees a
    l'adresse physique fixe 0x7E00 par boot/loader/stage2.asm
    (voir le commentaire de detection VBE dans ce fichier), et
    expose des primitives de dessin de base.

    IMPORTANT -- ce module NE CHANGE PAS le mode video actif.
    stage2.asm ne fait que detecter et enregistrer un mode
    graphique candidat, sans jamais appeler la fonction BIOS de
    changement de mode (INT10h AX=0x4F02). Tant que ce
    changement de mode reel n'a pas ete ajoute, la carte video
    reste en mode texte, et ces primitives de dessin ne
    produisent donc actuellement aucun effet visible -- elles
    posent les bases, pretes a l'emploi des que la bascule
    reelle sera ajoutee (voir le README, section "Interface
    graphique").
*/


/*
    Couleur au format 0xRRGGBB (24 bits utiles, independant de
    la profondeur reelle du mode video -- gfx_put_pixel()
    convertit vers le format natif du framebuffer).
*/

typedef u32 gfx_color;


#define GFX_BLACK   0x000000
#define GFX_WHITE   0xFFFFFF
#define GFX_CYAN    0x00E5FF
#define GFX_DARKBLUE 0x0A1128
#define GFX_GRAY    0x888888


void graphics_init();


/*
    Renvoie 1 si un mode graphique lineaire a ete detecte au
    demarrage (voir stage2.asm) ET que ce module a pu s'initialiser
    correctement, 0 sinon (aucune des autres fonctions de ce
    fichier ne doit etre appelee si gfx_available() renvoie 0).
*/

int gfx_available();


u32 gfx_width();

u32 gfx_height();


void gfx_put_pixel(u32 x, u32 y, gfx_color color);

void gfx_fill_rect(u32 x, u32 y, u32 w, u32 h, gfx_color color);

void gfx_draw_rect(u32 x, u32 y, u32 w, u32 h, gfx_color color);

void gfx_draw_hline(u32 x, u32 y, u32 length, gfx_color color);

void gfx_draw_vline(u32 x, u32 y, u32 length, gfx_color color);


/*
    Dessine un caractere ASCII (0-127) via la police 8x8
    (assets/fonts/font8x8_basic.h), agrandi par un facteur
    "scale" (1 = 8x8 pixels reels, 2 = 16x16, etc).
*/

void gfx_draw_char(u32 x, u32 y, char c, gfx_color color, u32 scale);

void gfx_draw_text(u32 x, u32 y, const char* text, gfx_color color, u32 scale);


void gfx_clear(gfx_color color);


/*
    Fait defiler le framebuffer vers le haut de "pixel_rows"
    lignes de pixels (la zone du haut est perdue), et vide la
    bande laissee libre en bas avec "bg". Utilise par la
    console graphique (kernel/console/console.c) quand le
    curseur atteint le bas de l'ecran.
*/

void gfx_scroll_up(u32 pixel_rows, gfx_color bg);


#endif
