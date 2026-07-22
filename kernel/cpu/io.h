#ifndef MIKEA_IO_H
#define MIKEA_IO_H


#include "../../include/types.h"


/*
    Acces direct aux ports d'entree/sortie x86 (in/out).
    Necessaire pour piloter le PIC 8259, le PIT et le
    controleur clavier PS/2. Fonctions "static inline" pour
    eviter tout appel de fonction depuis un contexte
    d'interruption critique.
*/

static inline void outb(u16 port, u8 value)
{

asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));

}


static inline u8 inb(u16 port)
{

u8 value;

asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));

return value;

}


static inline void outw(u16 port, u16 value)
{

asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));

}


static inline u16 inw(u16 port)
{

u16 value;

asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));

return value;

}


/*
    Courte pause utilisee apres certaines ecritures de
    ports (ex. remappage du PIC) pour laisser au materiel
    le temps de traiter la commande sur du materiel reel
    (moins necessaire sous QEMU, mais c'est la pratique
    standard).
*/

static inline void io_wait(void)
{

outb(0x80, 0);

}


#endif
