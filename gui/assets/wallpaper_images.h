#ifndef MIKEA_GUI_WALLPAPER_IMAGES_H
#define MIKEA_GUI_WALLPAPER_IMAGES_H


#include "../../include/types.h"


/*
    ============================================================
    Mikea OS - Photos de fond d'ecran (donnees embarquees)
    ============================================================

    Contrairement aux motifs procéduraux de gui/theme.c
    (WALLPAPER_GRADIENT et les autres, toujours disponibles), ce
    fichier contient de VRAIES photos, fournies par l'utilisateur
    et converties une seule fois, hors ligne, en pixels RVB bruts
    (voir gui/assets/wallpaper_images.c, genere automatiquement
    par un script Python -- ne jamais modifier ce fichier a la
    main). Aucun decodeur JPEG/PNG n'existe dans ce noyau (voir
    le commentaire de gui/theme.h) : c'est la seule maniere de
    faire apparaitre une vraie photo a l'ecran ici -- le pixel
    brut est directement compile DANS le noyau, comme une police
    de caracteres l'est deja (assets/fonts/font8x8_basic.h).

    Compromis assumes, dictes par la place reservee au noyau sur
    le disque de demarrage (600 secteurs = 300 Ko, voir
    boot/loader/stage2.asm, dap_kernel) :
    - Resolution fixe de 80x60 pixels par photo (~14 Ko chacune,
      ~84 Ko pour les 6) -- volontairement petite pour rester
      tres en dessous de cette limite. Affichee "en mosaique"
      (chaque pixel source devient un bloc de plusieurs pixels a
      l'ecran, voir gui_paint_photo_area(), gui/desktop.c) plutot
      qu'une image lissee -- honnete sur la resolution reelle
      plutot que de la dissimuler.
    - RVB brut (3 octets/pixel), aucune compression : le plus
      simple et le plus sur a decoder correctement sans jamais
      risquer de plante sur un fichier corrompu -- au prix d'une
      taille un peu plus grande qu'avec une compression.
    ============================================================
*/


typedef struct
{

const char* name;

const unsigned char* rgb_data; /* width*height*3 octets, ligne par ligne, R,G,B par pixel */

u32 width;

u32 height;

} wallpaper_photo;


extern const wallpaper_photo WALLPAPER_PHOTOS[];


/* Nombre de photos disponibles dans WALLPAPER_PHOTOS[]. */

u32 wallpaper_photo_count();


#endif
