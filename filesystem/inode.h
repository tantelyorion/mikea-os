/*
====================================================

        Mikea OS Filesystem

        Inode Manager

        MKFS v2


        Developer:
        Tantely Orion


====================================================
*/


#ifndef MIKEA_INODE_H
#define MIKEA_INODE_H


#include "../include/types.h"



/*
====================================================
CONFIGURATION
====================================================
*/


#define MAX_INODES 128



#define FILE_NAME_SIZE 32




/*
====================================================
INODE STRUCTURE
====================================================
*/


typedef struct
{


/*
Unique identifier

*/

u32 id;



/*
File name

*/

char name[FILE_NAME_SIZE];



/*
File size

*/

u32 size;



/*
First data block

*/

u32 block;



/*
File status

*/

int used;



/*
    Champ reserve pour un futur systeme de permissions PAR
    FICHIER (actuellement fixe a 7 = rwx a la creation, voir
    inode_create() dans inode.c, et jamais relu ailleurs).

    Le controle d'acces reellement applique aujourd'hui est
    global PAR UTILISATEUR (security/permission.c,
    filesystem/file.c::current_user_can() /
    filesystem/directory.c::current_user_can()) : un
    utilisateur avec PERMISSION_WRITE peut modifier n'importe
    quel fichier -- il n'existe pas encore de notion de
    "proprietaire" par inode. Ne pas considerer ce champ comme
    applique tant qu'un tel modele (avec un champ owner_id)
    n'aura pas ete ajoute et branche dans file.c/directory.c.

Permission

*/

u32 permission;



} inode;





/*
====================================================
API
====================================================
*/


void inode_init();



inode* inode_create(
char* name
);



inode* inode_find(
char* name
);



void inode_delete(
char* name
);



inode* inode_get(
u32 id
);



u32 inode_count();



#endif