#include "process.h"



static process processes[MAX_PROCESS];

static u32 process_count = 0;



void process_init()
{


for(int i=0;i<MAX_PROCESS;i++)
{


processes[i].pid=0;

processes[i].state=
PROCESS_STOPPED;


}


process_count=0;


}



process* process_create(
char* name,
void (*entry)()
)
{


if(process_count>=MAX_PROCESS)
{

return 0;

}



process* p =
&processes[process_count];



p->pid =
process_count+1;



p->state =
PROCESS_READY;



p->entry =
entry;



int i=0;


while(name[i] && i<31)
{

p->name[i]=name[i];

i++;

}


p->name[i]=0;



process_count++;



return p;


}




process* process_get(
u32 pid
)
{


/*
    Correctif : pid=0 n'est jamais attribue par
    process_create() (les pid commencent a 1), mais
    c'est aussi la valeur par defaut de tous les
    emplacements non utilises (process_init()). Sans
    ce garde-fou, process_get(0) renvoyait le premier
    emplacement libre comme s'il s'agissait d'un
    processus reel.
*/

if(pid==0)
{

return 0;

}


for(int i=0;i<MAX_PROCESS;i++)
{


if(processes[i].pid==pid)
{

return &processes[i];

}


}



return 0;


}



u32 process_get_count()
{


return process_count;


}