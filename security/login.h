#ifndef MIKEA_LOGIN_H
#define MIKEA_LOGIN_H


#include "user.h"


/*
    Affiche l'invite de connexion ("login:" / "password:") et
    bloque jusqu'a ce qu'un couple identifiant/mot de passe
    valide soit saisi (voir security/user.c : user_login()).
    Recommence indefiniment tant que la saisie est incorrecte.

    Renvoie l'utilisateur desormais connecte (jamais 0 : cette
    fonction ne rend la main qu'apres une authentification
    reussie).
*/

user* login_prompt();


#endif
