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



/*
    Nombre de processus enregistres depuis le demarrage
    (bornee par MAX_PROCESS). Permet de parcourir la table
    avec process_get(1..process_get_count()) puisque les pid
    sont attribues sequentiellement par process_create() et ne
    sont jamais recycles -- meme principe que
    user_slot_count()/user_get_slot() dans security/user.h.
    Utilisee par la commande shell "ps" (shell/commands.c).
*/

u32 process_get_count();



#endif