#ifndef MIKEA_HEAP_H
#define MIKEA_HEAP_H


#include "../../include/types.h"


void heap_init();


void* mk_malloc(u32 size);


void mk_free(void* ptr);


#endif