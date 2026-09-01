#include "installer.h"

#include "../../kernel/cpu/io.h"


void console_write(const char* text);

unsigned long timer_ticks();


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
    Selection ESCLAVE + LBA. Correctif (meme cause que
    ATA_MASTER_SELECT plus bas) : le bit mode LBA (0x40) est
    ici explicitement positionne (0xF0), contrairement a
    filesystem/disk.c qui utilise 0xB0 avec succes -- la
    difference concrete est que ce fichier VERIFIE
    immediatement, octet par octet, chaque ecriture par une
    relecture (voir installer_run() plus bas), un usage plus
    strict qui a revele un souci que le seul usage de disk.c
    (jamais verifie de la sorte) ne faisait jamais remonter.
*/

#define ATA_SLAVE_SELECT 0xF0

/*
    Correctif (cause reelle de l'installation qui echoue) :
    0xA0 (sans le bit mode LBA, bit6) fonctionnait par chance
    la plupart du temps, mais un diagnostic bas niveau (lecture
    du registre d'erreur ATA, code ABRT persistant) a montre
    que le disque MAITRE -- et LUI SEUL, le disque utilise par
    le BIOS pour demarrer -- attend ce bit correctement positionne
    pour interpreter les registres Sector/Cylinder comme une
    adresse LBA plutot que comme une adresse CHS historique (le
    disque esclave, jamais implique dans le demarrage, tolere
    lui les deux). 0xE0 = maitre + bit mode LBA explicitement
    positionne.
*/

#define ATA_MASTER_SELECT 0xE0


#define SECTOR_SIZE 512


#define ATA_TIMEOUT_ITERATIONS 100000


/*
    Delai de stabilisation reel apres un changement de disque
    selectionne (registre Drive/Head), voir ata_read_sector()/
    ata_write_sector() plus bas. Remplace un ancien delai fixe
    de quatre lectures de Status ignorees (quelques dizaines de
    nanosecondes tout au plus, le minimum theorique exige par
    la specification ATA/IDE) par une attente basee sur le
    minuteur systeme (timer_ticks(), 100 Hz -- voir
    kernel/drivers/timer/timer.c).

    2 ticks (20 ms) : valeur determinee empiriquement (pas une
    estimation) -- 1 tick (10 ms) laissait encore l'installation
    echouer de temps a autre au bout de plusieurs centaines de
    secteurs (echec intermittent, signature typique d'une
    course de timing), tandis que 2 ticks se sont montres
    fiables sur plusieurs installations completes (2048
    secteurs, verifies octet par octet) d'affilee. Le cout est
    une installation plus lente (plusieurs dizaines de secondes
    au lieu de quelques secondes, chaque secteur declenchant
    jusqu'a trois changements de disque) -- un compromis
    largement justifie pour une operation destructrice qui ne
    se produit qu'une fois, avec une barre de progression a
    l'ecran (voir apps/installer_app/installer_app.c).
*/

static void ata_delay_ticks(unsigned long ticks)
{

unsigned long start = timer_ticks();

while (timer_ticks() - start < ticks)
{

inb(ATA_STATUS);

}

}


static int ata_wait_ready()
{

/*
    Correctif (installation qui echoue de facon intermittente
    -- cause reelle confirmee par diagnostic bas niveau) :
    cette fonction ne verifiait que le bit BSY (occupe), jamais
    le bit ERR (erreur). Consequence concrete dans
    ata_write_sector() plus bas : un probleme survenant PENDANT
    la phase interne d'ecriture du disque (apres le transfert
    des 512 octets, ou pendant FLUSH CACHE) n'etait jamais
    detecte -- BSY retombait bien a 0 (le disque n'est plus
    "occupe"), mais ERR restait actif, et ata_write_sector()
    renvoyait quand meme un succes. La RELECTURE DE
    VERIFICATION qui suivait immediatement (voir installer_run())
    heritait alors de ce code d'erreur jamais nettoye et
    echouait aussitot, quel que soit le secteur -- exactement
    le symptome observe ("erreur de lecture"/"verification"
    signale au premier secteur, juste apres l'ecriture
    correspondante).
*/

for (u32 tries = 0; tries < ATA_TIMEOUT_ITERATIONS; tries++)
{

u8 status = inb(ATA_STATUS);

if (status & ATA_STATUS_BSY)
{

continue;

}

if (status & ATA_STATUS_ERR)
{

return -1;

}

return 0;

}

return -1;

}


static int ata_wait_data()
{

/*
    Correctif (conformite ATA/IDE -- voir le wiki OSDev, "ATA
    PIO Mode") : attendre que BSY retombe a 0 avant de tester
    ERR/DRQ. En pratique, la plupart des controleurs (dont
    celui de QEMU) ne mettent jamais DRQ ou ERR a 1 tant que
    BSY vaut encore 1, donc l'ancien code (qui ne testait que
    ERR/DRQ) fonctionnait deja correctement ici -- mais rien ne
    garantit ce comportement sur tout materiel/emulateur, et la
    specification est explicite sur l'ordre a respecter.
*/

for (u32 tries = 0; tries < ATA_TIMEOUT_ITERATIONS; tries++)
{

u8 status = inb(ATA_STATUS);

if (status & ATA_STATUS_BSY)
{

continue;

}

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

/*
    Correctif (ordre des operations) : le disque doit etre
    SELECTIONNE D'ABORD, puis on attend qu'IL soit pret --
    pas l'inverse. Une version anterieure de ce code appelait
    ata_wait_ready() AVANT de selectionner le disque, ce qui
    ne verifiait donc que l'etat du disque UTILISE PAR
    L'OPERATION PRECEDENTE (potentiellement l'autre disque),
    jamais celui qu'on s'appretait reellement a utiliser.
*/

outb(ATA_DRIVE_HEAD, (u8)(drive_select | ((lba >> 24) & 0x0F)));


/*
    Delai de stabilisation (~400ns exiges par la specification
    ATA/IDE apres tout changement de Drive/Head) -- voir
    ata_delay_ticks() plus haut.
*/

ata_delay_ticks(2);


if (ata_wait_ready() != 0)
{

return -1;

}


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

/* Correctif (meme ordre que ata_read_sector() ci-dessus) : selection du disque avant l'attente, pas apres. */

outb(ATA_DRIVE_HEAD, (u8)(drive_select | ((lba >> 24) & 0x0F)));


/* Meme delai de stabilisation requis qu'ata_read_sector() ci-dessus. */

ata_delay_ticks(2);


if (ata_wait_ready() != 0)
{

return -1;

}


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


install_result installer_run(void (*progress_callback)(u32 sector, u32 total), u32* failed_sector_out)
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

if (failed_sector_out != (void*)0) { *failed_sector_out = sector; }

return INSTALL_ERROR_READ;

}


if (ata_write_sector(sector, src_buffer, ATA_SLAVE_SELECT) != 0)
{

console_write("[install] Erreur d'ecriture (disque esclave), secteur ");

write_u32(sector);

console_write("\n");

if (failed_sector_out != (void*)0) { *failed_sector_out = sector; }

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

if (failed_sector_out != (void*)0) { *failed_sector_out = sector; }

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

if (failed_sector_out != (void*)0) { *failed_sector_out = sector; }

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
