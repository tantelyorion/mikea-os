#ifndef MIKEA_PROCESS_H
#define MIKEA_PROCESS_H


#include "../../include/types.h"



#define MAX_PROCESS 32



typedef enum
{

PROCESS_READY,

PROCESS_RUNNING,

PROCESS_STOPPED


}process_state;




typedef struct
{


u32 pid;


char name[32];


process_state state;



void (*entry)();



}process;




void process_init();


process* process_create(
char* name,
void (*entry)()
);



process* process_get(
u32 pid
);



#endif