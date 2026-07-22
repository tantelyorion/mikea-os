#include "heap.h"



#define HEAP_START 0x100000


#define HEAP_SIZE  0x100000


#define HEAP_END   (HEAP_START + HEAP_SIZE)



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

static block_header* block_list;



void heap_init()
{

    heap_pointer = (u8*)HEAP_START;

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

    if (heap_pointer + total > (u8*)HEAP_END)
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

    if ((u8*)block < (u8*)HEAP_START || (u8*)block >= heap_pointer)
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
