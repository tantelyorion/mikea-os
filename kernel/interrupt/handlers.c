#include "handlers.h"

#include "pic.h"

#include "irq.h"

#include "../process/thread.h"

#include "../drivers/timer/timer.h"

#include "../drivers/keyboard/keyboard.h"



void isr_handler(u64 vector)
{

(void)vector;


/*
    Avant ce fichier, aucune exception CPU n'etait
    interceptee correctement (l'IDT n'etait meme pas
    chargee). Une exception grave (division par zero,
    acces memoire invalide...) provoquait un triple fault
    et redemarrait la machine sans aucune information.

    Pour l'instant, on arrete proprement le CPU plutot que
    de continuer dans un etat potentiellement corrompu.
    Une prochaine etape possible : afficher le numero de
    vecteur et un message de diagnostic avant l'arret.
*/

irq_disable();

while (1)
{

asm volatile("hlt");

}

}



void irq_handler(u64 vector)
{

u8 irq = (u8)(vector - 32);


if (irq == 0)
{

/* IRQ0 : PIT, notre horloge systeme. */

timer_tick();

}
else if (irq == 1)
{

/* IRQ1 : clavier PS/2. */

keyboard_handle_irq();

}


/* Il faut TOUJOURS signaler la fin d'IRQ, sinon le PIC */
/* ne delivre plus jamais aucune interruption ensuite.  */

pic_send_eoi(irq);


if (irq == 0)
{

/*
    Preemption reelle : avant ce fichier, seul un thread
    qui appelait lui-meme thread_yield() rendait la main
    aux autres (cooperatif pur). Desormais, le timer force
    un point de bascule a chaque tick, meme si le thread en
    cours ne cede jamais volontairement la main.
*/

thread_yield();

}

}
