/*
====================================================

        Mikea Shell

        Version : 0.3

        File :
        msh.c


        Developer :
        Tantely Orion


====================================================
*/


#include "msh.h"

#include "commands.h"

#include "../security/login.h"

#include "../security/user.h"




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


/*
    Correctif : "logout" doit pouvoir ramener a l'ecran de
    connexion sans redemarrer le noyau. Comme execute_command()
    (shell/commands.c) ne renvoie rien au shell, la commande
    "logout" pose ce drapeau (via shell_request_logout(), voir
    msh.h) au lieu de casser directement la boucle ci-dessous
    depuis un autre fichier.
*/

static int logout_requested = 0;


void shell_request_logout()
{

logout_requested = 1;

}



void msh_start()
{


char command[128];


char prompt[64];


/*
    Correctif majeur : le shell demarrait directement, sans
    la moindre authentification, avec une invite
    "Tantely@Mikea:~$ " codee en dur. Le systeme
    utilisateur (security/user.c, user_login()) existait
    deja mais n'etait jamais appele nulle part -- voir
    security/login.c pour le detail. On bloque desormais ici
    tant qu'une connexion valide n'a pas eu lieu, et l'invite
    reflete ensuite le nom reel de l'utilisateur connecte.

    Correctif complementaire : cette authentification n'avait
    lieu qu'une seule fois (le shell restait connecte pour
    toujours ensuite). La boucle exterieure ci-dessous permet
    a "logout" de revenir ici sans redemarrer le noyau -- un
    autre compte peut alors se connecter dans la meme session.
*/

while(1)
{


logout_requested = 0;


user* logged_in = login_prompt();



console_write(
"================================\n"
);


console_write(
"        Mikea Shell v0.3\n"
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



/*
    Construit l'invite "utilisateur@Mikea:~$ " a partir du
    nom d'utilisateur reellement connecte, au lieu du nom
    "Tantely" fige en dur quel que soit l'utilisateur.
*/

int i=0;

while(logged_in->username[i] && i < 30)
{

prompt[i]=logged_in->username[i];

i++;

}

prompt[i]=0;


/*
    mk_strcat n'existe pas dans libc/string.c (voir la note
    a ce sujet) : on complete la chaine caractere par
    caractere, ce qui reste simple ici vu la taille fixe du
    suffixe.
*/

const char* suffix = "@Mikea:~$ ";

int j=0;

while(suffix[j] && i < 62)
{

prompt[i]=suffix[j];

i++;

j++;

}

prompt[i]=0;



while(!logout_requested)
{


console_write(
prompt
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


/*
    "logout" a ete tape : on deconnecte reellement
    l'utilisateur (voir security/user.c) avant de reboucler
    vers login_prompt(), sinon user_get_current() continuerait
    de renvoyer l'ancien utilisateur pendant l'ecran de
    connexion suivant.
*/

user_logout();

console_write(
"\nLogging out...\n\n"
);


}


}
