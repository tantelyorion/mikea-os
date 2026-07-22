#ifndef MIKEA_PASSWORD_H
#define MIKEA_PASSWORD_H


#include "../include/types.h"


void password_init();


/*
    Hache un mot de passe en clair avec un sel donne.
    Remplace le stockage en clair. Ce n'est pas un
    algorithme cryptographique certifie (pas de
    SHA-256/bcrypt disponible en environnement
    freestanding), mais il empeche la lecture directe
    des mots de passe et le contournement trivial qui
    existait dans user_login().
*/

u64 password_hash(const char* password, u32 salt);


#endif
