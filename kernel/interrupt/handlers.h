#ifndef MIKEA_HANDLERS_H
#define MIKEA_HANDLERS_H


#include "../../include/types.h"


/*
    Appele par isr_common_stub (kernel/cpu/isr_stubs.asm)
    pour toute exception CPU (vecteurs 0-31).
*/

void isr_handler(u64 vector);


/*
    Appele par irq_common_stub (kernel/cpu/isr_stubs.asm)
    pour toute IRQ materielle remappee (vecteurs 32-47).
*/

void irq_handler(u64 vector);


#endif
