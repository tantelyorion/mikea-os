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



/*
    Sauvegarde/recharge le bitmap des blocs (block_table, prive
    a ce fichier) sur/depuis le vrai disque -- voir fs_layout.h
    (FS_BLOCK_BITMAP_START_BLOCK) et mkfs.c pour l'orchestration
    complete avec inode_table_save()/load() (inode.c) et
    superblock_write()/load() (superblock.c). Correctif
    persistance disque : sans cela, le contenu des fichiers
    survivait deja sur le disque (voir block_write()/
    block_read() ci-dessous) mais devenait orphelin a chaque
    redemarrage, faute de savoir quels blocs etaient occupes.
*/

void block_table_save();

void block_table_load();



#endif