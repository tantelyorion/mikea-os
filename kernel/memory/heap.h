#ifndef MIKEA_HEAP_H
#define MIKEA_HEAP_H


#include "../../include/types.h"


void heap_init();


void* mk_malloc(u32 size);


void mk_free(void* ptr);


/*
    Statistiques du tas, pour affichage (voir apps/settings,
    section "Systeme"). "used_out"/"total_out" en OCTETS.
    "total_out" est la taille fixe HEAP_SIZE (heap.c) ; 
    "used_out" additionne les blocs actuellement NON libres de
    la liste chainee (block_list, heap.c) -- ne compte donc pas
    la portion jamais encore allouee au-dela de heap_pointer, ni
    les blocs libres reutilisables, seulement ce qui est
    reellement occupe la` maintenant.
*/

void heap_stats(u32* used_out, u32* total_out);


#endif