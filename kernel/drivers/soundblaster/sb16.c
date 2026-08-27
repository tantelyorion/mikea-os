#include "sb16.h"

#include "../../cpu/io.h"

#include "../speaker/speaker.h"


void console_write(const char* text);

unsigned long timer_ticks();

void* mk_memcpy(void* dest, const void* src, u32 len);


/*
    Ports fixes historiques (base 0x220, IRQ5, DMA8=1) : reglages
    par defaut utilises par QEMU sans configuration particuliere
    (voir "-device sb16"), et jumpers les plus courants sur le
    materiel reel d'origine.
*/

#define SB_BASE 0x220

#define DSP_RESET        (SB_BASE + 0x6)
#define DSP_READ_DATA    (SB_BASE + 0xA)
#define DSP_WRITE        (SB_BASE + 0xC)
#define DSP_WRITE_STATUS (SB_BASE + 0xC)
#define DSP_READ_STATUS  (SB_BASE + 0xE)


/*
    Controleur DMA 8237 n°1 (canaux 0-3, 8 bits) : registres pour
    le canal 1 specifiquement (adresse/compteur/page), plus les
    registres communs aux 4 canaux (masque/mode/flip-flop).
*/

#define DMA1_CHAN1_ADDR  0x02
#define DMA1_CHAN1_COUNT 0x03
#define DMA1_CHAN1_PAGE  0x83

#define DMA1_MASK        0x0A
#define DMA1_MODE        0x0B
#define DMA1_FLIPFLOP    0x0C


static int g_sb16_present = 0;


/*
    Tampon de lecture DMA dedie : le controleur 8237 (canaux 8
    bits) ne peut pas transferer un bloc qui chevauche une
    frontiere de 64 Ko en memoire physique. Plutot que d'exiger cet
    alignement des donnees sources embarquees (sound_data.c,
    placees par le linker sans contrainte particuliere), on copie
    toujours vers ce tampon interne, aligne sur 64 Ko et strictement
    plus petit qu'une page de 64 Ko : n'importe quelle donnee qui y
    est copiee reste donc necessairement entierement a l'interieur
    d'une seule frontiere.

    65536 octets couvrent largement le plus gros des deux sons
    systeme embarques (shutdown_pcm, 55125 octets -- voir
    sound_data.h).
*/

#define DMA_BUFFER_SIZE 65536

static u8 dma_buffer[DMA_BUFFER_SIZE] __attribute__((aligned(65536)));


static void dsp_write(u8 value)
{

/*
    Bit 7 du port de statut d'ecriture = 1 tant que le DSP n'est
    pas pret a recevoir un nouvel octet -- meme genre d'attente
    active que speaker_beep()/le reste du noyau, jamais plus de
    quelques microsecondes sur du materiel reel comme sous QEMU.
*/

u32 timeout = 100000;

while ((inb(DSP_WRITE_STATUS) & 0x80) != 0 && timeout > 0)
{

timeout--;

}


outb(DSP_WRITE, value);

}


int sb16_init()
{


/*
    Sequence de reset standard du DSP (identique sur toute carte
    de la famille Sound Blaster, documentee sur le wiki OSDev,
    "Sound Blaster 16" -- section "DSP Reset") : ecrire 1 puis 0
    sur le port de reset, avec une breve pause entre les deux,
    puis attendre que le DSP reponde 0xAA sur le port de lecture.
*/

outb(DSP_RESET, 1);


/*
    Pause d'au moins ~3 microsecondes exigee par le materiel entre
    les deux ecritures -- io_wait() (ecriture vers le port 0x80,
    deja utilisee ailleurs dans le noyau pour la meme raison, voir
    kernel/interrupt/pic.c) répétée fournit une pause largement
    suffisante sans dependre d'un minuteur pas encore initialise a
    ce stade du demarrage.
*/

for (int i = 0; i < 16; i++)
{

io_wait();

}


outb(DSP_RESET, 0);


u32 timeout = 100000;

while ((inb(DSP_READ_STATUS) & 0x80) == 0 && timeout > 0)
{

timeout--;

}


if (timeout == 0)
{

console_write("Sound Blaster 16 non detectee (delai de reset).\n");

g_sb16_present = 0;

return 0;

}


u8 response = inb(DSP_READ_DATA);

if (response != 0xAA)
{

console_write("Sound Blaster 16 non detectee.\n");

g_sb16_present = 0;

return 0;

}


console_write("Sound Blaster 16 detectee (DSP pret).\n");

g_sb16_present = 1;

return 1;

}


int sb16_available()
{

return g_sb16_present;

}


/*
    Programme le controleur DMA 8237 (canal 1, 8 bits, transfert
    "read" -- memoire vers peripherique, c'est-a-dire une LECTURE
    depuis le point de vue de la carte son qui lit en memoire pour
    la jouer) sur "length" octets a partir de l'adresse physique
    "phys_addr". Sequence standard (wiki OSDev, "ISA DMA") : masquer
    le canal, reinitialiser le bit de bascule interne (flip-flop,
    partage par les ecritures 16 bits d'adresse ET de compteur),
    programmer mode/adresse/page/compteur, puis demasquer le canal.
*/

static void dma_setup_channel1(u32 phys_addr, u32 length)
{

u32 count = length - 1;


/* Masque le canal 1 (bits 0-1 = canal, bit 2 = 1 -> "masque"). */

outb(DMA1_MASK, 0x05);


outb(DMA1_FLIPFLOP, 0x00);


/*
    Registre de mode : canal 1, transfert "read" (memoire ->
    peripherique, valeur binaire 10 sur les bits 2-3), mode
    "single" (un seul cycle, valeur binaire 01 sur les bits 6-7),
    pas d'auto-init, adresse croissante -- octet 0x49 (voir le
    detail du calcul en commentaire de sb16.h).
*/

outb(DMA1_MODE, 0x49);


outb(DMA1_CHAN1_ADDR, (u8)(phys_addr & 0xFF));

outb(DMA1_CHAN1_ADDR, (u8)((phys_addr >> 8) & 0xFF));


outb(DMA1_CHAN1_PAGE, (u8)((phys_addr >> 16) & 0xFF));


outb(DMA1_FLIPFLOP, 0x00);


outb(DMA1_CHAN1_COUNT, (u8)(count & 0xFF));

outb(DMA1_CHAN1_COUNT, (u8)((count >> 8) & 0xFF));


/* Demasque le canal 1 (bit 2 = 0 -> "actif"). */

outb(DMA1_MASK, 0x01);

}


void sb16_play_pcm(const u8* data, u32 length, u32 sample_rate_hz)
{

if (!g_sb16_present)
{

return;

}


if (!sound_is_enabled())
{

return;

}


if (length == 0 || length > DMA_BUFFER_SIZE)
{

return;

}


/*
    Copie vers le tampon dedie aligne sur 64 Ko (voir le
    commentaire de dma_buffer plus haut) -- les donnees sources
    (sound_data.c) n'ont elles-memes aucune garantie d'alignement.
*/

mk_memcpy(dma_buffer, data, length);


dma_setup_channel1((u32)(u64)dma_buffer, length);


/* Allume le haut-parleur de la carte (compatibilite Sound Blaster historique ; sans effet nuisible sur une SB16). */

dsp_write(0xD1);


/* Frequence d'echantillonnage de sortie (commande SB16, deux octets poids fort puis poids faible). */

dsp_write(0x41);

dsp_write((u8)((sample_rate_hz >> 8) & 0xFF));

dsp_write((u8)(sample_rate_hz & 0xFF));


/*
    Lecture DMA 8 bits, cycle unique : commande 0xC0, puis un
    octet de mode (0x00 = mono, non signe -- format de
    sound_data.c), puis la longueur moins un sur deux octets
    (poids faible puis poids fort).
*/

u32 count = length - 1;

dsp_write(0xC0);

dsp_write(0x00);

dsp_write((u8)(count & 0xFF));

dsp_write((u8)((count >> 8) & 0xFF));


/*
    Attente bloquante de la duree du son (meme technique que
    speaker_beep(), kernel/drivers/speaker/speaker.c) : le
    minuteur systeme avance a 100 Hz (10 ms/tick), donc
    "duree_ms / 10" ticks -- ici calcule directement en evitant
    une division flottante (absente de ce noyau freestanding) :
    duree_ms = length * 1000 / sample_rate_hz, donc
    ticks = length * 100 / sample_rate_hz.
*/

unsigned long ticks_needed = ((unsigned long)length * 100) / sample_rate_hz;

if (ticks_needed == 0)
{

ticks_needed = 1;

}


unsigned long start = timer_ticks();

while (timer_ticks() - start < ticks_needed)
{

}


dsp_write(0xD3);

}
