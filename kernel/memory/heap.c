#include "heap.h"



/*
    ============================================================
    CORRECTIF CRITIQUE
    ============================================================

    HEAP_START valait auparavant 0x100000 -- exactement
    KERNEL_BASE dans linker.ld, l'adresse physique OU LE NOYAU
    LUI-MEME EST CHARGE (.text/.rodata/.data/.bss). Le tas
    demarrait donc au tout debut de l'image du noyau en cours
    d'execution, pas apres. Des le premier mk_malloc() (le test
    memoire de kernel_start(), quelques lignes apres
    heap_init()), l'ecriture "MIKEA OS" partait droit dans le
    code/donnees du noyau -- une corruption memoire quasi
    immediate et non deterministe, bien avant meme d'atteindre
    le shell.

    Le tas doit commencer APRES la fin de l'image chargee du
    noyau. "__kernel_end" est deja fourni par linker.ld
    (section .bss finale) : on s'en sert ici comme point de
    depart reel, aligne sur une frontiere de page (4096
    octets) par simple hygiene (pas une obligation stricte
    pour ce tas, mais evite tout partage de page avec le
    noyau).
*/

extern u8 __kernel_end;


#define HEAP_SIZE 0x100000


/*
    En-tete place devant chaque bloc alloue.
    Les blocs liberes restent dans la liste et sont
    reutilises (first-fit) au lieu d'etre perdus.
*/

typedef struct block_header
{

    u32 size;

    int free;

    struct block_header* next;

} block_header;



static u8* heap_pointer;

static u8* heap_start;

static u8* heap_end;

static block_header* block_list;



void heap_init()
{

    u64 raw = (u64)&__kernel_end;

    /* Aligne vers le haut sur une frontiere de 4096 octets. */

    u64 aligned = (raw + 0xFFF) & ~((u64)0xFFF);

    heap_start = (u8*)aligned;

    heap_pointer = heap_start;

    heap_end = heap_start + HEAP_SIZE;

    block_list = (void*)0;

}



void* mk_malloc(u32 size)
{

    if (size == 0)
    {
        return (void*)0;
    }


    /*
        1. Chercher d'abord un bloc deja libere
        et assez grand (first-fit). Corrige la fuite
        memoire de la version precedente ou mk_free()
        ne faisait rien.
    */

    block_header* current = block_list;

    while (current)
    {
        if (current->free && current->size >= size)
        {
            current->free = 0;
            return (void*)(current + 1);
        }

        current = current->next;
    }


    /*
        2. Sinon, etendre le tas. On verifie desormais
        que le nouveau bloc reste dans les limites de
        HEAP_SIZE : avant ce correctif, mk_malloc()
        pouvait ecrire au-dela du tas sans jamais
        detecter le depassement.
    */

    u32 total = (u32)sizeof(block_header) + size;

    if (heap_pointer + total > heap_end)
    {
        /* Tas plein : plus de memoire disponible. */
        return (void*)0;
    }

    block_header* block = (block_header*)heap_pointer;

    heap_pointer += total;

    block->size = size;

    block->free = 0;

    block->next = block_list;

    block_list = block;

    return (void*)(block + 1);

}



void mk_free(void* ptr)
{

    if (ptr == (void*)0)
    {
        return;
    }


    block_header* block = ((block_header*)ptr) - 1;


    /*
        Validation minimale : le bloc doit se trouver
        dans la zone effectivement utilisee du tas.
        Protege contre un appel avec un pointeur invalide.
    */

    if ((u8*)block < heap_start || (u8*)block >= heap_pointer)
    {
        return;
    }


    block->free = 1;

    /*
        Limite connue : pas de fusion (coalescing) des
        blocs libres adjacents pour l'instant, donc de
        la fragmentation reste possible sur le long terme.
        A ameliorer dans une prochaine version.
    */

}
