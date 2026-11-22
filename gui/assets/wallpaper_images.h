#ifndef MIKEA_GUI_WALLPAPER_IMAGES_H
#define MIKEA_GUI_WALLPAPER_IMAGES_H

#include "../../include/types.h"

/*
    ============================================================
    Mikea OS - Photos de fond d'ecran
    ============================================================

    Les pixels ne sont plus compiles dans le noyau : ils sont lus
    a la demande depuis le pack d'assets du disque de demarrage
    (kernel/assets, outil tools/pack_assets.py). Une seule photo
    est gardee en memoire a la fois (la derniere demandee).

    Les fichiers sources sont dans le dossier assets/wallpapers (fichiers .rgb)
    (RVB brut, largeur*hauteur*3 octets, aucun decodeur requis).
    ============================================================
*/

typedef struct
{
    const char* name;                  /* nom affiche */
    const unsigned char* rgb_data;     /* width*height*3 octets, R,G,B par pixel */
    u32 width;
    u32 height;
} wallpaper_photo;

/* Nombre de photos declarees (disponibles si le pack est present). */
u32 wallpaper_photo_count();

/*
    Charge (si besoin) et renvoie la photo d'indice "index", ou NULL
    si l'indice est invalide ou si la lecture a echoue (pack absent...).
    Un echec est memorise pour ne pas relire le disque a chaque image.
    Le pointeur reste valide jusqu'au prochain appel avec un AUTRE indice.
*/
const wallpaper_photo* wallpaper_photo_get(u32 index);

#endif
