/*
====================================================

        Mikea Shell

        Version : 0.2

        File :
        msh.c


        Developer :
        Tantely Orion


====================================================
*/


#include "msh.h"

#include "commands.h"




/*
====================================================
EXTERNAL SYSTEMS
====================================================
*/


void console_write(
    const char* text
);



void input_readline(
    char* buffer,
    unsigned int max_len
);





/*
====================================================

        MIKEA SHELL START

====================================================
*/


void msh_start()
{


char command[128];




console_write(
"================================\n"
);


console_write(
"        Mikea Shell v0.2\n"
);


console_write(
"================================\n"
);


console_write(
"\n"
);



console_write(
"Welcome to Mikea OS\n"
);


console_write(
"Type help for commands\n"
);


console_write(
"\n"
);




while(1)
{


console_write(
"Tantely@Mikea:~$ "
);




/*
--------------------------------

Read user command

--------------------------------
*/


input_readline(
command,
sizeof(command)
);




/*
--------------------------------

Execute command

--------------------------------
*/


execute_command(
command
);



console_write(
"\n"
);



}


}