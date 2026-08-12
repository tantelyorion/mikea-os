#include "login.h"



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


/*
    ============================================================
    DIAGNOSTIC TEMPORAIRE -- A RETIRER une fois le probleme de
    connexion confirme resolu (voir le README, section
    Depannage). Affiche en clair exactement ce qui a ete capture
    pour le nom d'utilisateur et le mot de passe, afin de
    distinguer une faute de frappe invisible (mot de passe
    masque par des '*') d'un probleme plus profond.
*/

console_write("[diagnostic] identifiant capture: [");
console_write(username);
console_write("]\n");

console_write("[diagnostic] mot de passe capture: [");
console_write(password);
console_write("]\n");


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
