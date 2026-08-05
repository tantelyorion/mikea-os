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


#endif