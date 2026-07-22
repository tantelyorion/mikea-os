#include "mpm.h"

#include "package.h"



void console_write(
const char* text
);




void mpm_init()
{


package_init();


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



console_write(
"Package removed\n"
);



}




void mpm_list()
{


console_write(
"Installed packages\n"
);


}