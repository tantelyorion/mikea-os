#include "scheduler.h"

#include "process.h"

#include "thread.h"



void scheduler_init()
{


process_init();


}



void scheduler_run()
{

/*
    Avant ce correctif, scheduler_run() etait un stub
    ("while(1){break;}") qui ne faisait litteralement
    rien : aucun thread n'etait jamais execute.

    Desormais, tant qu'il existe des threads actifs
    (crees via thread_create()), on cede le CPU en boucle
    au round-robin cooperatif. Chaque thread doit appeler
    thread_yield() periodiquement pour laisser tourner
    les autres (pas encore de preemption par interruption
    timer : ce sera la prochaine etape).
*/

while (1)
{

thread_yield();


/*
    Si aucun thread n'est pret, thread_yield() revient
    immediatement : on evite de saturer le CPU pour rien
    en laissant la place a une interruption materielle.
*/

asm volatile("hlt");

}

}
