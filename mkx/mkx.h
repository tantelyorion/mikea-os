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



/*
    Correctif stabilite (saut hors tampon) : "entry" et "size"
    viennent tous les deux de l'en-tete du fichier lui-meme,
    donc entierement controlables par quiconque ecrit ce
    fichier. mkx_execute() ne verifiait "entry" que par
    rapport a "size" -- si "size" est declare artificiellement
    grand (fichier corrompu ou malveillant), "entry" pouvait
    alors depasser tres largement le tampon memoire reellement
    alloue pour ce fichier (voir filesystem/file.h,
    MAX_FILE_SIZE) sans que rien ne le detecte, et le
    programme sautait executer de la memoire noyau arbitraire
    -- plantage quasi certain, potentiellement pire.
    "buffer_size" est desormais fourni par l'appelant (qui
    connait la taille reelle du tampon d'ou vient "program",
    ex. MAX_FILE_SIZE pour un fichier) et sert de limite
    absolue, en plus de la coherence interne "entry" < "size".
*/

int mkx_execute(
mkx_header* program,
u32 buffer_size
);



#endif