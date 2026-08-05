#ifndef MIKEA_DIRECTORY_H
#define MIKEA_DIRECTORY_H



void directory_init();



/*
    Renvoie 1 si le repertoire a ete cree, 0 si le nom est
    invalide ou deja utilise (voir la note dans directory.c
    sur les limites actuelles du modele de repertoires).
*/

int directory_create(
char* name
);



#endif