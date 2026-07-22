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



#endif