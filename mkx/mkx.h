#ifndef MIKEA_MKX_H
#define MIKEA_MKX_H


#include "../include/types.h"



#define MKX_MAGIC 0x4D4B58



typedef struct
{


u32 magic;


u32 version;


u32 entry;


u32 size;



}mkx_header;




void mkx_init();


/*
    Correctif (code non branche) : cette fonction (mkx/runtime.c)
    etait definie mais n'apparaissait meme pas dans ce header --
    aucun fichier ne pouvait donc l'appeler. Elle prepare
    l'environnement d'execution des applications MKX (pour
    l'instant un stub : voir mkx/runtime.c pour les etapes
    futures -- appels systeme, isolation memoire...). On
    l'appelle desormais depuis mkx_init(), comme le reste de
    l'initialisation du module mkx.
*/

void mkx_runtime_start();



int mkx_execute(
mkx_header* program
);



#endif