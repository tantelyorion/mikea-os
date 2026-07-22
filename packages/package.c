#include "package.h"



static package packages[MAX_PACKAGES];

static u32 package_count=0;




void package_init()
{


for(int i=0;i<MAX_PACKAGES;i++)
{


packages[i].installed=0;


}



package_count=0;


}





package* package_create(
char* name,
char* version
)
{


if(package_count>=MAX_PACKAGES)
{

return 0;

}



package* pkg;


pkg=&packages[package_count];



pkg->id=
package_count+1;



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




package_count++;



return pkg;


}