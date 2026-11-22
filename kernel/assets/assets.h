#ifndef MIKEA_ASSETS_H
#define MIKEA_ASSETS_H

#include "../../include/types.h"

/*
    ============================================================
    Mikea OS - Assets sur disque (fonds d'ecran, sons...)
    ============================================================

    Les donnees lourdes ne sont plus compilees dans le noyau
    (limite de 500 Ko du chargeur, voir boot/loader/stage2.asm) :
    elles sont lues a la demande depuis le "pack" place sur le
    disque de demarrage a partir du secteur ASSETS_PAK_LBA.
    Format et outil : tools/pack_assets.py.

    Tout echec (pack absent, en-tete invalide, erreur disque)
    est SILENCIEUX et non fatal : les fonctions renvoient 0/NULL
    et l'appelant retombe sur son repli (degrade de couleurs,
    bip du haut-parleur...).

    Limite connue : la lecture passe par le disque MAITRE, comme
    l'installeur. Elle n'est pas protegee contre un acces
    simultane au disque de donnees depuis un autre thread.
    ============================================================
*/

#define ASSETS_PAK_LBA 1100

typedef struct
{
    u32 size;     /* octets */
    u16 width;    /* 0 pour un son */
    u16 height;
} asset_info;

/* 1 si l'asset existe, 0 sinon. Lit l'en-tete au premier appel. */
int assets_find(const char* name, asset_info* info);

/*
    Charge l'asset dans un bloc mk_malloc() ; l'appelant le libere
    avec mk_free(). NULL en cas d'echec.
*/
u8* assets_load(const char* name, u32* size_out);

#endif
