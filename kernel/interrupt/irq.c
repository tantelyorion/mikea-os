#include "irq.h"

#include "pic.h"



void irq_init()
{

/*
    Avant ce fichier, irq_init() ne faisait rien : le PIC
    gardait sa configuration par defaut, qui fait entrer en
    collision les IRQ materielles (0-7) avec les exceptions
    CPU (vecteurs 8-15). Voir kernel/interrupt/pic.c pour le
    detail du remappage.
*/

pic_remap();

}



void irq_enable()
{

asm volatile("sti");

}



void irq_disable()
{

asm volatile("cli");

}
