#ifndef MIKEA_THREAD_H
#define MIKEA_THREAD_H



#include "../../include/types.h"



#define MAX_THREAD 64

#define THREAD_STACK_SIZE 16384



typedef enum
{

THREAD_READY,

THREAD_RUNNING,

THREAD_TERMINATED

} thread_state;



typedef struct
{


u32 id;


void (*function)();


int active;


thread_state state;


/*
    Pile dediee du thread (allouee sur le tas) et
    pointeur de sauvegarde du contexte (rsp) utilise
    par context_switch(). C'est ce qui manquait pour
    faire du vrai multitache : avant, il n'existait
    aucune pile ni aucun mecanisme de sauvegarde des
    registres par thread.
*/

u8* stack_base;

u64* stack_pointer;


} thread;




void thread_init();



thread* thread_create(
void (*function)()
);



/*
    Cede volontairement le CPU au thread suivant
    (ordonnancement round-robin cooperatif).
*/

void thread_yield();



thread* thread_current();



#endif
