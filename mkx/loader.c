#include "mkx.h"

#include "../security/user.h"

#include "../security/permission.h"



void console_write(
const char* text
);



/*
    ============================================================
    CORRECTIF SECURITE (faille de controle d'acces)
    ============================================================

    mkx_execute() ne verifiait absolument aucune permission avant
    de sauter dans le programme charge -- alors que
    security/permission.h definit precisement PERMISSION_EXEC
    dans ce but, et que permission_init() ne l'accorde par
    defaut qu'a root (id 1). useradd (shell/commands.c ->
    security/user.c) accorde PERMISSION_READ | PERMISSION_WRITE
    a tout nouveau compte, jamais PERMISSION_EXEC.

    Consequence avant ce correctif : n'importe quel utilisateur
    non-root, qui a donc le droit d'ecrire un fichier ("write",
    voir filesystem/file.c::file_write(), protege seulement par
    PERMISSION_WRITE), pouvait construire un en-tete MKX valide
    dans ce fichier puis l'executer avec "run" (shell/commands.c
    -> cmd_run() -> mkx_execute()) sans jamais posseder
    PERMISSION_EXEC. Le code du fichier s'execute directement au
    meme niveau de privilege que le noyau (voir sdk/mikea_sdk.h) :
    c'etait donc une elevation de privileges complete, et
    PERMISSION_EXEC n'etait en pratique jamais applique nulle
    part dans le systeme.

    Meme regle que filesystem/file.c/directory.c
    (current_user_can()) : hors contexte utilisateur (aucune
    connexion active, ex. pendant l'initialisation du noyau),
    on autorise ; sinon on applique la table de permissions
    reelle de l'utilisateur connecte.
*/

static int current_user_can(int permission)
{

user* u = user_get_current();

if (u == 0)
{

return 1;

}

return check_permission((int)u->id, permission);

}



int mkx_execute(
mkx_header* program
)
{


if(!current_user_can(PERMISSION_EXEC))
{


console_write(
"Permission denied: execute permission required\n"
);


return -1;


}


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
