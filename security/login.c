#include "login.h"

#include "../gui/login_screen.h"

#include "../kernel/drivers/graphics/graphics.h"



/*
    ============================================================
    CORRECTIF MAJEUR
    ============================================================

    Le systeme multi-utilisateurs (security/user.c,
    password.c, permission.c) etait entierement fonctionnel
    mais jamais utilise : user_login() n'etait appelee nulle
    part, si bien que kernel_start() lancait directement le
    shell (msh_start()) sans la moindre authentification. Tout
    tournait de fait dans le mode "contexte systeme" prevu par
    filesystem/file.c pour l'initialisation interne du noyau
    (aucun utilisateur connecte = tout autorise), en
    permanence -- comme si l'on etait root sans jamais l'avoir
    demande.

    Ce fichier fournit l'ecran de connexion manquant : il
    bloque au demarrage tant qu'un identifiant et un mot de
    passe valides n'ont pas ete saisis.
*/


void console_write(
const char* text
);


void input_readline(
char* buffer,
unsigned int max_len
);


void input_readline_secure(
char* buffer,
unsigned int max_len
);


void keyboard_flush();



user* login_prompt()
{


/*
    Ecran de connexion graphique (gui/login_screen.c) des
    qu'un mode graphique est disponible, au lieu de l'invite
    texte "Mikea OS login:"/"Password:" ci-dessous -- qui
    reste le seul recours si aucun mode graphique n'a ete
    detecte au demarrage (voir boot/loader/stage2.asm).
*/

if (gfx_available())
{

return gui_login_screen();

}


char username[32];

char password[32];


while(1)
{


console_write("\n");

console_write("Mikea OS login: ");

/*
    Correctif : voir le commentaire de keyboard_flush()
    (kernel/drivers/keyboard/keyboard.c) -- sans cet appel,
    des frappes accumulees pendant le demarrage (ou lors
    d'un essai precedent rate) pouvaient polluer la saisie
    du nom d'utilisateur qui suit, faisant echouer la
    connexion meme avec les bons identifiants.
*/

keyboard_flush();

input_readline(
username,
sizeof(username)
);


console_write("Password: ");

input_readline_secure(
password,
sizeof(password)
);


user* logged_in = user_login(username, password);


if(logged_in != 0)
{

console_write("\n");

return logged_in;

}


console_write(
"Login incorrect\n"
);


}


}
