#ifndef MIKEA_MSH_H
#define MIKEA_MSH_H


void msh_start();


/*
    Demande la deconnexion de l'utilisateur courant : la
    commande shell "logout" (voir shell/commands.c) appelle
    cette fonction, ce qui fait revenir msh_start() a l'ecran
    de connexion (security/login.c) des la fin de la commande
    en cours, sans redemarrer le noyau.
*/

void shell_request_logout();


/*
    Consomme (lit puis remet a zero) la demande de
    deconnexion posee par shell_request_logout() : renvoie 1
    une seule fois si "logout" a ete demande depuis le dernier
    appel, 0 sinon. Utilise par gui/desktop.c (terminal
    integre au bureau graphique), qui n'a pas acces a la
    boucle de msh_start() pour detecter directement la
    commande "logout".
*/

int shell_logout_was_requested();


#endif