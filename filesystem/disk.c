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


void console_write(
const char* text
);




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



/*
    Correctif stabilite (gel au demarrage) : cette boucle
    n'avait AUCUNE limite. disk_init() l'appelle des le tout
    debut du demarrage, avant meme l'ecran de connexion -- si
    le disque esclave (build/disk.img, voir la note en tete de
    fichier) n'est pas fourni comme second "-drive" a QEMU, ou
    absent/mal branche sur une machine reelle, le bit BSY
    pouvait ne jamais retomber a 0 et le systeme entier restait
    fige en silence, ecran fixe, sans le moindre message.

    ATA_TIMEOUT_ITERATIONS est une limite en nombre
    d'iterations (pas en temps reel : on evite ainsi de
    dependre d'une horloge deja initialisee), tres largement
    suffisante pour un disque reel (pret en quelques
    microsecondes) mais qui garantit que le noyau reprend
    toujours la main. Renvoie 0 si pret, -1 en cas de timeout.
*/

#define ATA_TIMEOUT_ITERATIONS 100000


static int ata_wait_ready()
{

/* Attend que le controleur ne soit plus occupe (bit BSY). */

for (u32 tries = 0; tries < ATA_TIMEOUT_ITERATIONS; tries++)
{

if ((inb(ATA_STATUS) & ATA_STATUS_BSY) == 0)
{
return 0;
}

}


return -1;

}



static int ata_wait_data()
{

/*
    Attend que les donnees soient pretes (bit DRQ), ou une
    erreur -- meme correctif de timeout que ata_wait_ready()
    ci-dessus, pour la meme raison (eviter un gel indefini si
    le disque ne repond jamais).
*/

for (u32 tries = 0; tries < ATA_TIMEOUT_ITERATIONS; tries++)
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


return -1;

}



static int ata_read_sector(u32 lba, u8* buffer)
{

if (ata_wait_ready() != 0)
{
return -1;
}


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

if (ata_wait_ready() != 0)
{
return -1;
}


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

if (ata_wait_ready() != 0)
{
return -1;
}

outb(ATA_COMMAND, ATA_CMD_FLUSH);

if (ata_wait_ready() != 0)
{
return -1;
}

return 0;

}



void disk_init()
{

/*
    Rien a "effacer" : contrairement au disque virtuel en
    RAM, un vrai disque garde son contenu entre deux demarrages
    (c'est justement le but). On se contente de s'assurer que
    le controleur est pret.

    Correctif stabilite : si le disque ne repond pas (absent,
    mal branche a QEMU...), on ne bloque plus indefiniment le
    demarrage (voir ata_wait_ready() ci-dessus). On previent
    clairement et on continue : le systeme demarre quand meme
    jusqu'au shell, simplement sans persistance disque
    fonctionnelle (chaque operation fichier echouera alors
    proprement plutot que de figer la machine).
*/

if (ata_wait_ready() != 0)
{

console_write(
"[disk] Aucune reponse du disque de donnees -- "
"persistance desactivee pour cette session\n"
);

}

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
