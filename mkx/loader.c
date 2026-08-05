#include "mkx.h"



void console_write(
const char* text
);



int mkx_execute(
mkx_header* program
)
{


if(program->magic != MKX_MAGIC)
{


console_write(
"Invalid MKX file\n"
);


return -1;


}


/*
    Correctif : avant ce fichier, mkx_execute() affichait
    "Execution OK" sans jamais executer la moindre
    instruction du programme charge -- le format MKX ne
    faisait donc absolument rien.

    "entry" est le decalage (en octets) du point d'entree du
    programme par rapport au debut de l'en-tete mkx_header.
    On verifie qu'il reste dans les limites declarees par
    "size" avant d'y sauter, pour eviter de sauter en dehors
    du programme charge si le fichier est corrompu ou
    malveillant.

    IMPORTANT (voir sdk/mikea_sdk.h) : il n'existe pas encore
    de separation memoire/privilege (pas de Ring 3). Le code
    du programme s'execute donc directement au meme niveau de
    privilege que le noyau, sans protection contre un
    programme qui deborderait de sa zone declaree autrement
    qu'en verifiant "entry" ici.
*/

if(program->entry >= program->size)
{


console_write(
"Invalid MKX entry point\n"
);


return -1;


}


console_write(
"Loading MKX program...\n"
);


void (*entry_point)() =
(void (*)())((u8*)program + program->entry);


entry_point();


console_write(
"Execution OK\n"
);


return 0;


}
