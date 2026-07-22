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