#include "commands.h"


void console_write(
    const char* text
);



void execute_command(
    char* command
)
{


if(command[0]=='h')
{

console_write(
"Available commands:\n"
);

console_write(
"help\n"
);

console_write(
"about\n"
);

console_write(
"version\n"
);

console_write(
"clear\n"
);

console_write(
"cpu\n"
);

console_write(
"mem\n"
);


}


else if(command[0]=='a')
{


console_write(
"Mikea OS\n"
);


console_write(
"Open Source Operating System\n"
);


console_write(
"Developer : Tantely Orion\n"
);


}


else if(command[0]=='v')
{


console_write(
"Mikea OS version 0.1.5\n"
);


}


else if(command[0]=='c')
{


console_write(
"Screen cleared\n"
);


}


else if(command[0]=='m')
{


console_write(
"Memory Manager OK\n"
);


}


else
{


console_write(
"Command not found\n"
);


}


}