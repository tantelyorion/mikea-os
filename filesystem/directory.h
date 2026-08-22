#ifndef MIKEA_DIRECTORY_H
#define MIKEA_DIRECTORY_H


#include "inode.h"



void directory_init();



/*
    Renvoie 1 si le repertoire a ete cree, 0 si le nom est
    invalide ou deja utilise (voir la note dans directory.c
    sur les limites actuelles du modele de repertoires).
    "parent" ("" pour la racine) est le dossier dans lequel ce
    nouveau dossier apparait -- voir directory_list_children().
*/

int directory_create(
char* name,
char* parent
);


/*
    Liste, dans "out_names"/"out_is_dir" (tableaux paralleles
    de "max_count" entrees), les elements (fichiers ET
    dossiers, non supprimes -- voir inode.h, champ "deleted")
    dont le parent est exactement "parent" ("" pour la
    racine). Renvoie le nombre d'elements ecrits.
*/

u32 directory_list_children(
char* parent,
char out_names[][FILE_NAME_SIZE],
int* out_is_dir,
u32 max_count
);



#endif