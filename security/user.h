#ifndef MIKEA_USER_H
#define MIKEA_USER_H


#include "../include/types.h"


#define MAX_USERS 32



typedef struct
{


u32 id;


char username[32];


/*
    Correctif securite : le mot de passe n'est plus
    stocke en clair. On garde uniquement son empreinte
    (hash) et le sel utilise pour la calculer.
*/

u64 password_hash;

u32 password_salt;


int active;


}user;




void user_system_init();



user* user_create(
char* username,
char* password
);



user* user_login(
char* username,
char* password
);


/*
    Renvoie l'utilisateur actuellement connecte, ou 0 si
    personne n'est connecte (ex. avant l'ecran de connexion,
    pendant l'initialisation du systeme).
*/

user* user_get_current();


#endif
