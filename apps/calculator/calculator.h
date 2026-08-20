#ifndef MIKEA_APP_CALCULATOR_H
#define MIKEA_APP_CALCULATOR_H

/*
    Calculatrice (application systeme par defaut, mode
    graphique uniquement). Deplacee depuis shell/commands.c
    vers son propre fichier pour une meilleure organisation --
    toujours compilee directement dans le noyau (pas un
    executable .mkx separe, voir apps/hello pour ce modele-la),
    appelee comme une commande shell classique.
*/

void cmd_calc();

#endif
