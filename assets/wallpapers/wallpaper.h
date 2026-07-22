#ifndef MIKEA_WALLPAPER_H
#define MIKEA_WALLPAPER_H


#include "../../include/types.h"


/*
    IMPORTANT :

    Le pilote video actuel (kernel/drivers/framebuffer.c)
    pilote la VGA en MODE TEXTE (0xB8000, grille de
    caracteres 80x25), pas un framebuffer graphique en
    pixels. Un "fond d'ecran" au sens image n'a donc rien
    a afficher pour l'instant.

    Ce fichier ne fait que reserver la couleur et le format
    prevus pour plus tard : quand un vrai pilote graphique
    (VESA/VBE ou GOP) existera, cette couleur pourra remplir
    un buffer de pixels RGB.
*/


#define WALLPAPER_WIDTH  320

#define WALLPAPER_HEIGHT 200


/* Bleu Mikea OS par defaut (R, G, B) */

#define WALLPAPER_COLOR_R 0x1A

#define WALLPAPER_COLOR_G 0x3C

#define WALLPAPER_COLOR_B 0x6E



/*
    Remplit un buffer RGB (WALLPAPER_WIDTH * WALLPAPER_HEIGHT
    pixels, 3 octets par pixel) d'une couleur unie. A appeler
    depuis le futur pilote graphique une fois qu'il existera.
*/

static inline void wallpaper_fill_solid(u8* rgb_buffer)
{

u32 pixel_count = WALLPAPER_WIDTH * WALLPAPER_HEIGHT;


for (u32 i = 0; i < pixel_count; i++)
{

rgb_buffer[i*3 + 0] = WALLPAPER_COLOR_R;
rgb_buffer[i*3 + 1] = WALLPAPER_COLOR_G;
rgb_buffer[i*3 + 2] = WALLPAPER_COLOR_B;

}

}


#endif
