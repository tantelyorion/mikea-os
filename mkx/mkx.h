#ifndef MIKEA_MKX_H
#define MIKEA_MKX_H


#include "../include/types.h"



#define MKX_MAGIC 0x4D4B58



typedef struct
{


u32 magic;


u32 version;


u32 entry;


u32 size;



}mkx_header;




void mkx_init();



int mkx_execute(
mkx_header* program
);



#endif