#ifndef MIKEA_PACKAGE_H
#define MIKEA_PACKAGE_H


#include "../include/types.h"



#define MAX_PACKAGES 128



typedef struct
{


u32 id;


char name[32];


char version[16];


int installed;



}package;



void package_init();



package* package_create(
char* name,
char* version
);



/*
    Recherche un paquet installe par son nom.
    Renvoie 0 si aucun paquet installe ne correspond.
*/
package* package_find(
char* name
);



/*
    Desinstalle un paquet par son nom (installed = 0).
    Renvoie 1 si le paquet a ete trouve et desinstalle,
    0 sinon (paquet inconnu ou deja desinstalle).
*/
int package_delete(
char* name
);



/*
    Nombre total d'emplacements utilises dans la table
    des paquets (installes ou non). A utiliser avec
    package_get() pour parcourir tous les paquets.
*/
u32 package_count();



/*
    Acces direct a un paquet par son index dans la table
    (0 <= index < package_count()). Renvoie 0 si l'index
    est hors limites.
*/
package* package_get(
u32 index
);



#endif