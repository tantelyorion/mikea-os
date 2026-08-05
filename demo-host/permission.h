#ifndef MIKEA_PERMISSION_H
#define MIKEA_PERMISSION_H


#include "types.h"


#define PERMISSION_READ   1
#define PERMISSION_WRITE  2
#define PERMISSION_EXEC   4




void permission_init();



/*
    Verifie si l'utilisateur "user" (son id, voir security/user.h)
    possede la ou les permissions demandees (masque de bits,
    ex. PERMISSION_READ | PERMISSION_WRITE).

    Correctif : l'ancienne version renvoyait toujours 1 (tout
    est permis, pour tout le monde, tout le temps) -- un
    "controle d'acces" qui n'en controlait aucun.
*/

int check_permission(
int user,
int permission
);



/*
    Accorde une permission a un utilisateur. Reservee pour
    l'instant a une utilisation interne/administrative (pas
    encore reliee a une commande shell).
*/

void permission_grant(
int user,
int permission
);


#endif
