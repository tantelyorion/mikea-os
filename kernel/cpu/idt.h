#ifndef MIKEA_IDT_H
#define MIKEA_IDT_H


#include "../../include/types.h"


void idt_init();


/*
    Enregistre une entree dans la table (vecteur 0-255,
    adresse du gestionnaire, selecteur de segment de code,
    octet d'attributs). Expose pour permettre a d'autres
    modules d'ajouter des gestionnaires plus tard sans
    modifier idt.c.
*/

void idt_set_gate(u8 vector, u64 handler, u16 selector, u8 flags);


#endif
