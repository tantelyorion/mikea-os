#include "input.h"

#include "../drivers/keyboard/keyboard.h"

#include "../console/console.h"



/*
    Correctif majeur (historique) : input_readline() lisait
    bien les caracteres tapes (keyboard_getchar()) et les
    stockait dans "buffer", mais ne les affichait jamais a
    l'ecran. L'utilisateur tapait donc une commande a
    l'aveugle, sans aucun retour visuel (pas d'echo), et la
    touche Retour arriere ('\b') n'etait pas geree du tout.

    Fonction interne commune a input_readline() (echo en
    clair) et input_readline_secure() (echo masque par '*',
    utilisee pour les mots de passe -- voir security/login.c).
*/

static void input_readline_impl(
char* buffer,
unsigned int max_len,
int mask
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

console_write("\n");

break;

}


if(c=='\b')
{

/* Retour arriere : n'efface que ce qui a ete tape ici. */

if(index > 0)
{

index--;

console_backspace();

}

continue;

}


/*
    Avant ce changement, aucune limite n'etait verifiee et
    l'ecriture continuait au-dela du buffer appelant (ex.
    command[128] dans msh.c), provoquant un depassement de
    pile. On reserve un octet pour le terminateur nul final.
*/

if(index >= (int)max_len - 1)
{

continue;

}


buffer[index]=c;

index++;


/*
    Echo : affiche le caractere tape immediatement, ou une
    etoile a la place si "mask" est actif (mot de passe).
*/

char echo[2];

echo[0] = mask ? '*' : c;

echo[1]=0;

console_write(echo);


}


}



void input_readline(
char* buffer,
unsigned int max_len
)
{

input_readline_impl(buffer, max_len, 0);

}



void input_readline_secure(
char* buffer,
unsigned int max_len
)
{

input_readline_impl(buffer, max_len, 1);

}
