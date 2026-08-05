#ifndef MIKEA_INPUT_H
#define MIKEA_INPUT_H


void input_readline(
char* buffer,
unsigned int max_len
);


/*
    Identique a input_readline(), mais affiche une etoile '*'
    a la place de chaque caractere tape au lieu de l'afficher
    en clair. A utiliser pour la saisie d'un mot de passe
    (voir security/login.c).
*/

void input_readline_secure(
char* buffer,
unsigned int max_len
);


#endif
