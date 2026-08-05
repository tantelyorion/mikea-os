/*
====================================================

        Mikea OS Filesystem

        Pilote disque ATA PIO (bus primaire, disque esclave)

        Version:
        MKFS v3

====================================================
*/


#include "disk.h"

#include "../kernel/cpu/io.h"



/*
====================================================
PORTS ATA (bus primaire)
====================================================

Avant ce fichier, le "disque" etait un simple tableau en
RAM (virtual_disk[DISK_SIZE]) : toutes les donnees du
systeme de fichiers etaient perdues au redemarrage. Ce
fichier pilote desormais un vrai disque via le port I/O
ATA en mode PIO (polling, sans IRQ14 pour rester simple).

On utilise le disque ESCLAVE du bus primaire (et non le
maitre) car le maitre est deja occupe par l'image de
demarrage (boot+stage2+noyau) fournie a QEMU en premier
disque. Le disque de donnees (build/disk.img, cree par le
Makefile) doit etre fourni comme DEUXIEME "-drive" a QEMU
pour apparaitre a cet emplacement (voir la cible "run" du
Makefile).
*/


#define ATA_DATA         0x1F0

#define ATA_ERROR        0x1F1

#define ATA_SECCOUNT     0x1F2

#define ATA_LBA_LOW      0x1F3

#define ATA_LBA_MID      0x1F4

#define ATA_LBA_HIGH     0x1F5

#define ATA_DRIVE_HEAD   0x1F6

#define ATA_STATUS       0x1F7

#define ATA_COMMAND      0x1F7


#define ATA_CMD_READ     0x20

#define ATA_CMD_WRITE    0x30

#define ATA_CMD_FLUSH    0xE7


#define ATA_STATUS_ERR   0x01

#define ATA_STATUS_DRQ   0x08

#define ATA_STATUS_BSY   0x80


/* 0xB0 = LBA + disque esclave (0xE0 serait le disque maitre). */

#define ATA_SLAVE_SELECT 0xB0



static void ata_wait_ready()
{

/* Attend que le controleur ne soit plus occupe (bit BSY). */

while (inb(ATA_STATUS) & ATA_STATUS_BSY)
{
}

}



static int ata_wait_data()
{

/* Attend que les donnees soient pretes (bit DRQ), ou une erreur. */

while (1)
{

u8 status = inb(ATA_STATUS);


if (status & ATA_STATUS_ERR)
{
return -1;
}


if (status & ATA_STATUS_DRQ)
{
return 0;
}

}

}



static int ata_read_sector(u32 lba, u8* buffer)
{

ata_wait_ready();


outb(ATA_DRIVE_HEAD, (u8)(ATA_SLAVE_SELECT | ((lba >> 24) & 0x0F)));

outb(ATA_SECCOUNT, 1);

outb(ATA_LBA_LOW,  (u8)(lba & 0xFF));

outb(ATA_LBA_MID,  (u8)((lba >> 8) & 0xFF));

outb(ATA_LBA_HIGH, (u8)((lba >> 16) & 0xFF));

outb(ATA_COMMAND, ATA_CMD_READ);


if (ata_wait_data() != 0)
{
return -1;
}


for (u32 i = 0; i < SECTOR_SIZE / 2; i++)
{

u16 word = inw(ATA_DATA);

buffer[i * 2]     = (u8)(word & 0xFF);

buffer[i * 2 + 1] = (u8)((word >> 8) & 0xFF);

}


return 0;

}



static int ata_write_sector(u32 lba, const u8* buffer)
{

ata_wait_ready();


outb(ATA_DRIVE_HEAD, (u8)(ATA_SLAVE_SELECT | ((lba >> 24) & 0x0F)));

outb(ATA_SECCOUNT, 1);

outb(ATA_LBA_LOW,  (u8)(lba & 0xFF));

outb(ATA_LBA_MID,  (u8)((lba >> 8) & 0xFF));

outb(ATA_LBA_HIGH, (u8)((lba >> 16) & 0xFF));

outb(ATA_COMMAND, ATA_CMD_WRITE);


if (ata_wait_data() != 0)
{
return -1;
}


for (u32 i = 0; i < SECTOR_SIZE / 2; i++)
{

u16 word = (u16)buffer[i * 2] | ((u16)buffer[i * 2 + 1] << 8);

outw(ATA_DATA, word);

}


/* Force l'ecriture reelle sur le disque (vide le cache du controleur). */

ata_wait_ready();

outb(ATA_COMMAND, ATA_CMD_FLUSH);

ata_wait_ready();


return 0;

}



void disk_init()
{

/*
    Rien a "effacer" : contrairement au disque virtuel en
    RAM, un vrai disque garde son contenu entre deux demarrages
    (c'est justement le but). On se contente de s'assurer que
    le controleur est pret.
*/

ata_wait_ready();

}



/*
    Lecture/ecriture d'un seul octet : implementees par un
    aller-retour sur le secteur entier qui le contient. C'est
    correct mais lent -- prefer disk_read_buffer()/
    disk_write_buffer() (utilisees par filesystem/block.c avec
    des blocs de 512 octets alignes sur un secteur) pour de
    meilleures performances.
*/

void disk_write(
u32 address,
u8 data
)
{

if (address >= DISK_SIZE)
{
return;
}


u32 lba = address / SECTOR_SIZE;

u32 offset = address % SECTOR_SIZE;


u8 sector[SECTOR_SIZE];


if (ata_read_sector(lba, sector) != 0)
{
return;
}


sector[offset] = data;


ata_write_sector(lba, sector);

}



u8 disk_read(
u32 address
)
{

if (address >= DISK_SIZE)
{
return 0;
}


u32 lba = address / SECTOR_SIZE;

u32 offset = address % SECTOR_SIZE;


u8 sector[SECTOR_SIZE];


if (ata_read_sector(lba, sector) != 0)
{
return 0;
}


return sector[offset];

}



void disk_write_buffer(
u32 address,
u8* buffer,
u32 size
)
{

if (address + size >= DISK_SIZE)
{
return;
}


/*
    Chemin rapide : ecriture alignee sur un secteur entier
    (cas de filesystem/block.c, blocs de 512 octets). On
    ecrit secteur par secteur directement, sans passer par
    un aller-retour de lecture inutile.
*/

if (address % SECTOR_SIZE == 0 && size % SECTOR_SIZE == 0)
{

u32 sectors = size / SECTOR_SIZE;

u32 base_lba = address / SECTOR_SIZE;


for (u32 i = 0; i < sectors; i++)
{

ata_write_sector(base_lba + i, buffer + (i * SECTOR_SIZE));

}


return;

}


/* Chemin general (non aligne) : octet par octet. */

for (u32 i = 0; i < size; i++)
{

disk_write(address + i, buffer[i]);

}

}



void disk_read_buffer(
u32 address,
u8* buffer,
u32 size
)
{

if (address + size >= DISK_SIZE)
{
return;
}


if (address % SECTOR_SIZE == 0 && size % SECTOR_SIZE == 0)
{

u32 sectors = size / SECTOR_SIZE;

u32 base_lba = address / SECTOR_SIZE;


for (u32 i = 0; i < sectors; i++)
{

ata_read_sector(base_lba + i, buffer + (i * SECTOR_SIZE));

}


return;

}


for (u32 i = 0; i < size; i++)
{

buffer[i] = disk_read(address + i);

}

}
