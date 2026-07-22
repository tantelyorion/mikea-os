/*
====================================================

        Mikea OS Filesystem

        Superblock Manager

        MKFS v2


        Developer:
        Tantely Orion


====================================================
*/


#ifndef MIKEA_SUPERBLOCK_H
#define MIKEA_SUPERBLOCK_H


#include "../include/types.h"



/*
====================================================
SUPERBLOCK CONSTANT
====================================================
*/


#define MIKEA_MAGIC "MIKEA"



/*
====================================================
SUPERBLOCK STRUCTURE
====================================================
*/


typedef struct
{


/*
Identification
*/

char magic[8];



/*
Filesystem information
*/

u32 version;


u32 block_size;


u32 total_blocks;


u32 free_blocks;



/*
Inode information
*/

u32 total_inodes;


u32 free_inodes;



/*
Status
*/

int mounted;



} superblock;





/*
====================================================
API
====================================================
*/


void superblock_init();



void superblock_write();



void superblock_load();



superblock* superblock_get();



#endif