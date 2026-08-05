/*
====================================================

        Mikea OS Filesystem

        File Manager

        MKFS v2


        Developer:
        Tantely Orion


====================================================
*/


#ifndef MIKEA_FILE_H
#define MIKEA_FILE_H


#include "../include/types.h"



/*
====================================================
FILE CONFIGURATION
====================================================
*/


#define MAX_FILE_SIZE 512




/*
====================================================
FILE API
====================================================
*/


void file_init();



int file_create(
char* name
);



int file_write(
char* name,
char* data
);



char* file_read(
char* name
);



int file_delete(
char* name
);



int file_exists(
char* name
);



#endif