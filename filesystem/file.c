/*
====================================================

        Mikea OS Filesystem

        File Manager


        MKFS v2


====================================================
*/


#include "file.h"


#include "inode.h"

#include "block.h"

#include "../security/user.h"

#include "../security/permission.h"






/*
====================================================
TEMP BUFFER

Cache lecture

====================================================
*/


static char read_buffer[MAX_FILE_SIZE];



/*
    Avant l'appel a user_login() (ex. pendant
    l'initialisation du systeme de fichiers au demarrage),
    aucun utilisateur n'est encore connecte : on autorise
    dans ce cas precis (contexte systeme), sinon on applique
    la table de permissions reelle de l'utilisateur connecte.
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






/*
====================================================
INITIALIZE FILE SYSTEM
====================================================
*/


void file_init()
{


for(
int i=0;
i<MAX_FILE_SIZE;
i++
)
{


read_buffer[i]=0;


}



}






/*
====================================================
CREATE FILE
====================================================
*/


int file_create(
char* name
)
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


inode* node;



node =
inode_create(name);



if(node==0)
{

return 0;

}



return 1;


}







/*
====================================================
WRITE FILE

Data -> Block

====================================================
*/


int file_write(
char* name,
char* data
)
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


inode* node;



node =
inode_find(name);



if(node==0)
{


node =
inode_create(name);



}



if(node==0)
{

return 0;

}




u8 buffer[512];



for(
int i=0;
i<512;
i++
)
{


buffer[i]=0;


}






int i=0;



while(
data[i]
&&
i<511
)
{


buffer[i]=data[i];


i++;


}



buffer[i]=0;




/*
Write block

*/

block_write(
node->block,
buffer
);



node->size=i;



return 1;


}







/*
====================================================
READ FILE

Block -> Memory

====================================================
*/


char* file_read(
char* name
)
{


inode* node;



node =
inode_find(name);



if(node==0)
{

return 0;

}




u8 buffer[512];



block_read(
node->block,
buffer
);




for(
int i=0;
i<512;
i++
)
{


read_buffer[i]=buffer[i];


}



return read_buffer;


}








/*
====================================================
DELETE FILE
====================================================
*/


int file_delete(
char* name
)
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


inode* node;



node =
inode_find(name);



if(node==0)
{

return 0;

}



inode_delete(name);



return 1;


}








/*
====================================================
CHECK FILE EXISTENCE
====================================================
*/


int file_exists(
char* name
)
{


inode* node;



node =
inode_find(name);



if(node)
{

return 1;

}



return 0;


}