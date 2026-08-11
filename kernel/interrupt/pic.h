#ifndef MIKEA_PIC_H
#define MIKEA_PIC_H


#include "../../include/types.h"


/*
    Reprogramme le PIC 8259 maitre/esclave pour faire
    correspondre les IRQ materielles 0-15 aux vecteurs
    d'interruption 32-47.

    Indispensable : par defaut, le PIC envoie les IRQ0-7 sur
    les vecteurs 8-15, qui entrent en collision avec les
    exceptions CPU x86 (8 = Double Fault, 13 = General
    Protection Fault, etc.). Sans ce remappage, la moindre
    interruption materielle (le timer, le clavier...) serait
    interpretee comme une exception CPU.
*/

void pic_remap();


/*
    Signale la fin de traitement d'une IRQ au(x) PIC(s).
    irq est le numero materiel (0-15), pas le vecteur
    d'interruption (32-47).
*/

void pic_send_eoi(u8 irq);


#endif
