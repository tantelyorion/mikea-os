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


for(int i=0;i<MAX_PROCESS;i++)
{


if(processes[i].pid==pid)
{

return &processes[i];

}


}



return 0;


}