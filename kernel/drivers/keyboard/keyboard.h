#ifndef MIKEA_KEYBOARD_H
#define MIKEA_KEYBOARD_H


void keyboard_init();


char keyboard_getchar();


int keyboard_available();


/*
    Appele par le gestionnaire d'IRQ1 (voir
    kernel/interrupt/handlers.c) a chaque interruption du
    controleur clavier PS/2. Lit le scancode sur le port
    0x60, le traduit en caractere ASCII et l'ajoute au
    tampon circulaire interne.
*/

void keyboard_handle_irq();


#endif
