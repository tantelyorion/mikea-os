#include "input.h"

#include "../drivers/keyboard/keyboard.h"



void input_readline(
char* buffer,
unsigned int max_len
)
{


int index=0;


while(1)
{


char c;


c=keyboard_getchar();



if(c==0)
{

continue;

}



if(c=='\n')
{

buffer[index]=0;

break;

}


/*
    Correctif : avant ce changement, aucune limite
    n'etait verifiee et l'ecriture continuait au-dela
    du buffer appelant (ex. command[128] dans msh.c),
    provoquant un depassement de pile. On reserve un
    octet pour le terminateur nul final.
*/

if(index >= (int)max_len - 1)
{

continue;

}


buffer[index]=c;


index++;


}


}
