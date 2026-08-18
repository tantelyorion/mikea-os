#include "installer.h"

#include "../../kernel/cpu/io.h"


void console_write(const char* text);


static void write_u32(u32 value)
{

char buffer[11];

int i = 0;


if (value == 0)
{

buffer[i++] = '0';

}


while (value > 0)
{

buffer[i++] = (char)('0' + (value % 10));

value /= 10;

}


while (i > 0)
{

i--;

char c[2];

c[0] = buffer[i];

c[1] = 0;

console_write(c);

}

}


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


/*
    Selection ESCLAVE + LBA : voir filesystem/disk.c pour la
    meme valeur (0xB0), deja validee sur QEMU/VirtualBox tout au
    long de ce projet. MAITRE utilise la meme famille de bits,
    seul le bit de selection de disque (0x10) change.
*/

#define ATA_SLAVE_SELECT 0xB0

#define ATA_MASTER_SELECT 0xA0


#define SECTOR_SIZE 512


#define ATA_TIMEOUT_ITERATIONS 100000


static int ata_wait_ready()
{

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


static int ata_read_sector(u32 lba, u8* buffer, u8 drive_select)
{

if (ata_wait_ready() != 0)
{

return -1;

}


outb(ATA_DRIVE_HEAD, (u8)(drive_select | ((lba >> 24) & 0x0F)));

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


static int ata_write_sector(u32 lba, const u8* buffer, u8 drive_select)
{

if (ata_wait_ready() != 0)
{

return -1;

}


outb(ATA_DRIVE_HEAD, (u8)(drive_select | ((lba >> 24) & 0x0F)));

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


install_result installer_run(void (*progress_callback)(u32 sector, u32 total))
{


u8 src_buffer[SECTOR_SIZE];

u8 verify_buffer[SECTOR_SIZE];


for (u32 sector = 0; sector < INSTALLER_SECTOR_COUNT; sector++)
{


if (ata_read_sector(sector, src_buffer, ATA_MASTER_SELECT) != 0)
{

console_write("[install] Erreur de lecture (disque maitre), secteur ");

write_u32(sector);

console_write("\n");

return INSTALL_ERROR_READ;

}


if (ata_write_sector(sector, src_buffer, ATA_SLAVE_SELECT) != 0)
{

console_write("[install] Erreur d'ecriture (disque esclave), secteur ");

write_u32(sector);

console_write("\n");

return INSTALL_ERROR_WRITE;

}


/*
    Verification immediate : on relit ce qu'on vient
    d'ecrire et on le compare octet par octet a la source,
    plutot que de supposer que l'ecriture a reussi. Cela
    permet de detecter tout de suite un probleme (ex.
    mauvais octet de selection de disque) sans avoir a
    redemarrer pour le decouvrir.
*/

if (ata_read_sector(sector, verify_buffer, ATA_SLAVE_SELECT) != 0)
{

console_write("[install] Erreur de verification (relecture esclave), secteur ");

write_u32(sector);

console_write("\n");

return INSTALL_ERROR_VERIFY;

}


for (u32 i = 0; i < SECTOR_SIZE; i++)
{

if (src_buffer[i] != verify_buffer[i])
{

console_write("[install] Verification echouee (donnees differentes), secteur ");

write_u32(sector);

console_write(", octet ");

write_u32(i);

console_write("\n");

return INSTALL_ERROR_VERIFY;

}

}


if (progress_callback != (void*)0)
{

progress_callback(sector, INSTALLER_SECTOR_COUNT);

}


}


return INSTALL_OK;


}
