/*
====================================================

        Mikea OS Filesystem

        Block Manager

        Version:
        MKFS v2


        Developer:
        Tantely Orion


====================================================
*/


#ifndef MIKEA_BLOCK_H
#define MIKEA_BLOCK_H


#include "../include/types.h"



/*
====================================================
BLOCK CONFIGURATION
====================================================
*/


#define BLOCK_SIZE 512


#define TOTAL_BLOCKS 2048



/*
====================================================
BLOCK API
====================================================
*/


void block_init();



void block_write(
    u32 block,
    u8* data
);



void block_read(
    u32 block,
    u8* buffer
);



int block_is_free(
    u32 block
);



u32 block_allocate();



void block_free(
    u32 block
);



#endif