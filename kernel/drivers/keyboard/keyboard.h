#ifndef MIKEA_KEYBOARD_H
#define MIKEA_KEYBOARD_H


void keyboard_init();


char keyboard_getchar();


int keyboard_available();


/*
    Vide le tampon circulaire interne : toute frappe deja en
    attente est jetee sans etre lue. A appeler avant tout point
    d'entree qui commence a lire une saisie critique (ex. l'ecran
    de connexion), pour ignorer les frappes accumulees pendant le
    demarrage plutot que de les consommer par erreur comme faisant
    partie de la saisie attendue.
*/

void keyboard_flush();


/*
    Appele par le gestionnaire d'IRQ1 (voir
    kernel/interrupt/handlers.c) a chaque interruption du
    controleur clavier PS/2. Lit le scancode sur le port
    0x60, le traduit en caractere ASCII et l'ajoute au
    tampon circulaire interne.
*/

void keyboard_handle_irq();


#endif
