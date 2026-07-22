#include "timer.h"

#include "../../cpu/io.h"



#define PIT_CHANNEL0 0x40

#define PIT_COMMAND  0x43


/*
    Frequence cible des interruptions du PIT (Hz). 100 Hz
    donne un point de bascule du round-robin toutes les
    10 ms : assez reactif pour un shell interactif, sans
    generer un volume d'interruptions excessif.
*/

#define PIT_FREQUENCY 100

#define PIT_BASE_FREQUENCY 1193182



static volatile unsigned long ticks = 0;



void timer_init()
{

ticks = 0;


/*
    Avant ce fichier, timer_init() ne programmait rien :
    le PIT restait sur sa configuration par defaut (environ
    18.2 Hz, mode BIOS) et surtout, l'IDT n'etait pas
    chargee, donc IRQ0 n'aurait de toute facon jamais
    declenche timer_tick().

    Mode 3 (onde carree), canal 0, diviseur calcule pour
    obtenir PIT_FREQUENCY interruptions par seconde.
*/

u16 divisor = (u16)(PIT_BASE_FREQUENCY / PIT_FREQUENCY);


outb(PIT_COMMAND, 0x36);

outb(PIT_CHANNEL0, (u8)(divisor & 0xFF));

outb(PIT_CHANNEL0, (u8)((divisor >> 8) & 0xFF));

}



unsigned long timer_ticks()
{

return ticks;

}



void timer_tick()
{

ticks++;

}
