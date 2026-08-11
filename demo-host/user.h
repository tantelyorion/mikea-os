#ifndef MIKEA_USER_H
#define MIKEA_USER_H


#include "types.h"


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


/*
    Deconnecte l'utilisateur courant (user_get_current()
    renverra 0 ensuite). Utilise par la commande shell
    "logout" (voir shell/msh.c) pour revenir a l'ecran de
    connexion sans redemarrer le noyau.
*/

void user_logout();


/*
    Cherche un compte actif par son nom d'utilisateur, sans
    verifier de mot de passe (contrairement a user_login()).
    Renvoie 0 si aucun compte actif ne correspond. Utilise
    par les commandes d'administration (passwd, userdel...).
*/

user* user_find(
char* username
);


/*
    Change le mot de passe d'un utilisateur deja existant
    (nouveau sel + nouveau hash, voir security/password.h).
    Renvoie 1 en cas de succes, 0 si "u" est invalide.
*/

int user_set_password(
user* u,
char* password
);


/*
    Desactive le compte "username" (il devient invisible a
    user_login()/user_find(), et sa place peut theoriquement
    etre reutilisee plus tard). Le compte root (id 1) ne peut
    jamais etre supprime : le systeme aurait sinon plus aucun
    compte capable d'administrer les autres. Renvoie 1 en cas
    de succes, 0 si le compte n'existe pas ou est root.
*/

int user_delete(
char* username
);


/*
    Nombre de coquilles utilisateur occupees depuis le
    demarrage (bornee par MAX_USERS). Un utilisateur
    desactive par user_delete() reste compte ici (sa place
    n'est pas recyclee) : utiliser le champ "active" pour
    savoir s'il faut l'afficher. Avec user_get_slot(), permet
    de parcourir tous les comptes (ex. commande "users").
*/

u32 user_slot_count();


/*
    Renvoie un pointeur vers la coquille utilisateur
    d'indice "index" (0 <= index < user_slot_count()), ou 0
    si l'indice est hors bornes. Ne pas supposer que le
    compte est actif : verifier le champ "active".
*/

user* user_get_slot(
u32 index
);


#endif
