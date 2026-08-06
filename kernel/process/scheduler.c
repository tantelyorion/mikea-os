#include "scheduler.h"

#include "process.h"

#include "thread.h"



void scheduler_init()
{


/*
    Correctif (nettoyage/robustesse) : scheduler_init()
    rappelait process_init(), alors que kernel.c l'appelle
    deja explicitement juste avant (voir la sequence
    "PROCESS INITIALIZATION" dans kernel_start()). Sans
    consequence tant que process_create() n'a pas encore ete
    appele a ce moment-la (c'est le cas aujourd'hui), mais
    process_init() remet process_count a 0 et efface toute la
    table : si un jour un processus etait enregistre avant
    scheduler_init() (ordre modifie par erreur dans
    kernel.c), cette reinitialisation cachee l'effacerait
    silencieusement. On retire cette double responsabilite :
    scheduler_init() ne s'occupe plus que de l'ordonnanceur,
    l'initialisation de la table des processus reste
    entierement du ressort de kernel_start() -> process_init().
*/


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
