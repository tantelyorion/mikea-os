/*
====================================================

        Mikea OS Filesystem

        Pilote disque ATA PIO (bus primaire, disque esclave)

        Version:
        MKFS v3

====================================================
*/


#ifndef MIKEA_DISK_H
#define MIKEA_DISK_H


#include "../include/types.h"



/*
====================================================
DISK CONFIGURATION
====================================================
*/


/*
    Taille exposee au reste du systeme de fichiers.
    Correspond a build/disk.img, cree par le Makefile et
    fourni a QEMU comme deuxieme disque (voir "make run").
    Si vous changez cette valeur, mettez aussi a jour la
    regle de creation de disk.img dans le Makefile.
*/

#define DISK_SIZE        (1024 * 1024)

#define SECTOR_SIZE      512



/*
====================================================
DISK API
====================================================
*/


void disk_init();



void disk_write(
    u32 address,
    u8 data
);



u8 disk_read(
    u32 address
);



void disk_write_buffer(
    u32 address,
    u8* buffer,
    u32 size
);



void disk_read_buffer(
    u32 address,
    u8* buffer,
    u32 size
);


#endif
