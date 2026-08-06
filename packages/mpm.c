#include "mpm.h"

#include "package.h"

#include "installer.h"

#include "database.h"

#include "../security/user.h"

#include "../security/permission.h"



void console_write(
const char* text
);



/*
    ============================================================
    CORRECTIF SECURITE (controle d'acces manquant)
    ============================================================

    mpm_install()/mpm_remove() ne verifiaient absolument aucune
    permission avant d'installer ou de desinstaller un paquet --
    contrairement a TOUTES les autres operations d'ecriture du
    systeme (fichiers/repertoires dans filesystem/file.c et
    directory.c, comptes utilisateur dans shell/commands.c), qui
    exigent au minimum PERMISSION_WRITE. N'importe quel compte
    connecte, meme sans la moindre permission accordee (voir
    permission_init()), pouvait donc modifier l'etat du systeme
    de paquets. On applique ici la meme regle que le reste du
    systeme (current_user_can() : hors contexte utilisateur,
    ex. pendant l'initialisation du noyau, on autorise ; sinon
    on applique la table de permissions reelle).
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



void mpm_init()
{


package_init();


/*
    Correctif : installer_init() et database_init() etaient
    definies mais jamais appelees nulle part dans le noyau
    -- du code mort. Elles restent des stubs ("Future" :
    extraction MPK, base /system/packages.db), mais on les
    relie desormais au demarrage du gestionnaire de paquets
    comme les autres sous-systemes, pour que l'initialisation
    du module "packages" soit complete et coherente avec le
    reste du noyau (chaque sous-module a son *_init() appele
    depuis le point d'entree de son module).
*/

installer_init();


database_init();


}





void mpm_install(
char* package
)
{


if(!current_user_can(PERMISSION_WRITE))
{

console_write(
"Permission denied: write permission required\n"
);

return;

}


console_write(
"Installing package...\n"
);



package_create(
package,
"1.0"
);



console_write(
"Installation complete\n"
);



}





void mpm_remove(
char* package
)
{


if(!current_user_can(PERMISSION_WRITE))
{

console_write(
"Permission denied: write permission required\n"
);

return;

}


console_write(
"Removing package...\n"
);


/*
    Correctif : mpm_remove() se contentait d'afficher
    des messages sans jamais desinstaller le paquet.
    package_delete() n'existait pas non plus avant ce
    correctif (ni package_find/package_count/package_get).
*/

if(package_delete(package))
{

console_write(
"Package removed\n"
);

}
else
{

console_write(
"Package not found\n"
);

}


}




void mpm_list()
{


console_write(
"Installed packages\n"
);


/*
    Correctif : mpm_list() n'affichait jamais les
    paquets reellement installes, seulement un en-tete.
*/

u32 total = package_count();

u32 i = 0;

int any = 0;

while(i < total)
{

package* pkg = package_get(i);

if(pkg != 0 && pkg->installed)
{

console_write("  - ");

console_write(pkg->name);

console_write(" (");

console_write(pkg->version);

console_write(")\n");

any = 1;

}

i++;

}

if(!any)
{

console_write("  (aucun)\n");

}


}