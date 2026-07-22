#include "thread.h"

#include "../memory/heap.h"



/*
    extern : implementee en assembleur dans
    context_switch.asm. Sauvegarde le contexte du
    thread courant et restaure celui du suivant.
*/

extern void context_switch(u64** old_sp_store, u64* new_sp);



static thread threads[MAX_THREAD];

static u32 thread_count = 0;


/* thread actuellement elu par le round-robin */

static u32 current_index = 0;

static thread* running_thread = 0;


/*
    Thread "noyau" implicite : represente le contexte
    d'origine (celui qui existait avant tout thread_create),
    utilise comme point de depart du round-robin.
*/

static thread kernel_thread;



/*
    Point d'entree commun a tous les threads crees par
    thread_create(). Appele via "ret" a la fin du premier
    context_switch() vers ce thread.
*/

void thread_trampoline()
{

thread* self = running_thread;


if (self && self->function)
{

self->function();

}


if (self)
{

self->state = THREAD_TERMINATED;

self->active = 0;

}


/*
    Un thread termine ne doit jamais "tomber" dans du
    code arbitraire : on cede indefiniment le CPU aux
    threads restants.
*/

while (1)
{

thread_yield();

}

}



void thread_init()
{


for (int i = 0; i < MAX_THREAD; i++)
{

threads[i].active = 0;

threads[i].state = THREAD_TERMINATED;

}


thread_count = 0;

current_index = 0;


kernel_thread.id = 0;

kernel_thread.active = 1;

kernel_thread.state = THREAD_RUNNING;

kernel_thread.function = (void*)0;

kernel_thread.stack_base = (void*)0;

kernel_thread.stack_pointer = (void*)0;


running_thread = &kernel_thread;


}



thread* thread_create(
void (*function)()
)
{


if (thread_count >= MAX_THREAD)
{

return 0;

}


thread* t = &threads[thread_count];


t->id = thread_count + 1;

t->function = function;

t->active = 1;

t->state = THREAD_READY;


t->stack_base = (u8*)mk_malloc(THREAD_STACK_SIZE);


if (t->stack_base == 0)
{

/* Plus de memoire disponible pour ce thread. */

t->active = 0;

return 0;

}


u64* sp = (u64*)(t->stack_base + THREAD_STACK_SIZE);


/*
    On construit une pile initiale que context_switch()
    saura "depiler" comme si le thread avait deja ete
    interrompu une premiere fois : 6 registres callee-saved
    (mis a zero) puis une adresse de retour qui pointe vers
    thread_trampoline().
*/

sp -= 1;
*sp = (u64)thread_trampoline;   /* adresse de retour (ret) */

sp -= 1; *sp = 0; /* rbp */
sp -= 1; *sp = 0; /* rbx */
sp -= 1; *sp = 0; /* r12 */
sp -= 1; *sp = 0; /* r13 */
sp -= 1; *sp = 0; /* r14 */
sp -= 1; *sp = 0; /* r15 */


t->stack_pointer = sp;


thread_count++;


return t;


}



thread* thread_current()
{

return running_thread;

}



void thread_yield()
{


if (thread_count == 0)
{

/* Aucun thread cree : rien a ordonnancer. */

return;

}


thread* previous = running_thread;


/*
    Recherche round-robin du prochain thread actif et
    non termine, en repartant juste apres celui en cours.
*/

u32 tries = 0;

thread* next = 0;


while (tries < thread_count)
{

current_index = (current_index + 1) % thread_count;

thread* candidate = &threads[current_index];


if (candidate->active && candidate->state != THREAD_TERMINATED)
{

next = candidate;

break;

}


tries++;

}


if (next == 0 || next == previous)
{

/* Aucun autre thread pret : on continue sur place. */

return;

}


if (previous && previous->state != THREAD_TERMINATED)
{

previous->state = THREAD_READY;

}


next->state = THREAD_RUNNING;

running_thread = next;


/*
    kernel_thread possede elle aussi un champ
    stack_pointer (initialise a 0) : context_switch()
    le remplit lui-meme via "mov [rdi], rsp" au premier
    appel, donc aucun cas particulier n'est necessaire.
*/

context_switch(&previous->stack_pointer, next->stack_pointer);


}
