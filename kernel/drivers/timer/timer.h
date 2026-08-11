#ifndef MIKEA_TIMER_H
#define MIKEA_TIMER_H


void timer_init();


unsigned long timer_ticks();


/*
    Appele par le gestionnaire d'IRQ0 (voir
    kernel/interrupt/handlers.c) a chaque interruption du
    PIT. Incremente le compteur de ticks.
*/

void timer_tick();


#endif
