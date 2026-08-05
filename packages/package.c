#include "package.h"

#include "../libc/string.h"



static package packages[MAX_PACKAGES];

static u32 package_table_count=0;




void package_init()
{


for(int i=0;i<MAX_PACKAGES;i++)
{


packages[i].installed=0;


}



package_table_count=0;


}





package* package_create(
char* name,
char* version
)
{


/*
    Correctif : si le paquet existe deja (meme si
    desinstalle), on reutilise son emplacement au lieu
    d'en creer un nouveau a chaque appel. Avant ce
    correctif, appeler mpm_install() deux fois sur le
    meme paquet remplissait la table de doublons.
*/

package* existing = package_find(name);

if(existing == 0)
{

int i=0;

while(i < (int)package_table_count)
{

if(packages[i].installed == 0 &&
   mk_strcmp(packages[i].name, name) == 0)
{

existing = &packages[i];

break;

}

i++;

}

}

if(existing != 0)
{

existing->installed = 1;

int i=0;

while(version[i] && i<15)
{

existing->version[i]=version[i];

i++;

}

existing->version[i]=0;

return existing;

}



if(package_table_count>=MAX_PACKAGES)
{

return 0;

}



package* pkg;


pkg=&packages[package_table_count];



pkg->id=
package_table_count+1;



pkg->installed=1;



int i=0;


while(name[i] && i<31)
{


pkg->name[i]=name[i];


i++;


}


pkg->name[i]=0;



i=0;


while(version[i] && i<15)
{


pkg->version[i]=version[i];


i++;


}


pkg->version[i]=0;




package_table_count++;



return pkg;


}



/*
====================================================
FIND PACKAGE BY NAME
====================================================
*/


package* package_find(
char* name
)
{


u32 i=0;


while(i < package_table_count)
{


if(packages[i].installed && mk_strcmp(packages[i].name, name) == 0)
{

return &packages[i];

}


i++;


}


return 0;


}



/*
====================================================
DELETE (UNINSTALL) PACKAGE BY NAME
====================================================
*/


int package_delete(
char* name
)
{


package* pkg = package_find(name);


if(pkg == 0)
{

return 0;

}


pkg->installed = 0;


return 1;


}



/*
====================================================
COUNT / ITERATION HELPERS
====================================================
*/


u32 package_count()
{

return package_table_count;

}



package* package_get(
u32 index
)
{

if(index >= package_table_count)
{

return 0;

}

return &packages[index];

}