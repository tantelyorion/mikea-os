#include "console.h"

#include "../drivers/framebuffer.h"



static int cursor_x=0;
static int cursor_y=0;



void console_init()
{

cursor_x=0;
cursor_y=0;

}



void console_write(
const char* text
)
{


while(*text)
{


if(*text=='\n')
{

cursor_x=0;
cursor_y++;

text++;

continue;

}


/*
    Correctif : la version precedente appelait
    fb_write() (qui reecrit toute la chaine restante)
    a chaque caractere, soit un travail O(n^2) pour
    une chaine de longueur n. On ecrit ici un seul
    caractere a la fois avec fb_put(), en O(n).
*/

fb_put(
cursor_x,
cursor_y,
*text,
0x0F
);


cursor_x++;

text++;


}


}
