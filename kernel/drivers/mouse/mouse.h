#ifndef MIKEA_MOUSE_H
#define MIKEA_MOUSE_H

#include "../../../include/types.h"


/*
    Pilote souris PS/2 (etape 3 vers un bureau a fenetres
    multiples). Utilise le meme controleur 8042 que le clavier
    (kernel/drivers/keyboard), sur son second port auxiliaire,
    signale via IRQ12.
*/

void mouse_init();


/*
    Appele par le gestionnaire d'IRQ12 (voir
    kernel/interrupt/handlers.c) a chaque interruption du
    controleur souris PS/2. Accumule les paquets de 3 octets et
    met a jour la position/les boutons.
*/

void mouse_handle_irq();


/*
    Position courante du curseur, bornee a la resolution du
    mode graphique actif (voir kernel/drivers/graphics). Non
    definie si aucun mode graphique n'est actif -- verifier
    gfx_available() avant d'utiliser ces valeurs pour du rendu.
*/

s32 mouse_get_x();

s32 mouse_get_y();


int mouse_left_pressed();

int mouse_right_pressed();

int mouse_middle_pressed();


/*
    Statut textuel de la derniere initialisation (voir
    mouse_init() dans mouse.c) -- utile pour diagnostiquer sans
    avoir a repérer un message de demarrage qui peut avoir
    defile hors ecran.
*/

const char* mouse_get_init_status();


#endif
