#include "mpm.h"

#include "package.h"

#include "installer.h"

#include "database.h"



void console_write(
const char* text
);




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