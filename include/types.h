#ifndef MIKEA_TYPES_H
#define MIKEA_TYPES_H


typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;


typedef signed int s32;


/*
    Correctif (build casse sous GCC >= 15) : "typedef int bool;"
    definissait un "bool" maison de la taille d'un int (4 octets).
    Depuis GCC 15, le standard C par defaut est passe de gnu17 a
    gnu23, qui reserve "bool" comme mot-cle, ce typedef devient
    alors une erreur de compilation.
*/
// typedef int bool;
#include <stdbool.h>


#define TRUE  1
#define FALSE 0


#define NULL ((void*)0)


#endif
