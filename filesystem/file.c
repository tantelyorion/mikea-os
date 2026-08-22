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

return file_create_in(name, "");

}



int file_create_in(
char* name,
char* parent
)
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


inode* node;



node =
inode_create_ex(name, parent, 0);



if(node==0)
{

return 0;

}


/*
    Correctif (nom "invisible" mais indisponible) : sans ce
    controle, creer un fichier portant le meme nom qu'un
    element actuellement dans la corbeille (voir file_trash()
    plus bas) reussirait silencieusement -- inode_create()
    trouve l'entree existante et la renvoie telle quelle,
    encore marquee "deleted", donc toujours absente des
    listages malgre le succes apparent de la creation. On
    bloque ce cas explicitement : il faut restaurer ou vider la
    corbeille avant de pouvoir reutiliser ce nom.
*/

if(node->deleted)
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


/*
    Meme regle que file_create() ci-dessus : un nom dans la
    corbeille n'est pas disponible en ecriture non plus, tant
    qu'il n'a pas ete restaure ou que la corbeille n'a pas ete
    videe.
*/

if(node->deleted)
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


/*
    Correctif : file_create()/file_write()/file_delete()
    exigent tous les trois PERMISSION_WRITE via
    current_user_can(), mais file_read() ne verifiait rien du
    tout -- n'importe quel utilisateur connecte, meme sans la
    moindre permission accordee (voir security/permission.c),
    pouvait lire le contenu de n'importe quel fichier avec
    "cat". On applique ici la meme regle en lecture.
*/

if(!current_user_can(PERMISSION_READ))
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


/*
    Meme regle que file_create()/file_write() ci-dessus : un
    fichier dans la corbeille n'est pas lisible normalement non
    plus (voir gui/file_manager pour la vue "Corbeille" dediee,
    qui ne montre que les noms, pas le contenu).
*/

if(node->deleted)
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



if(node && !node->deleted)
{

return 1;

}



return 0;


}



/*
====================================================
CORBEILLE
====================================================
*/


int file_trash(
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


if(node->deleted)
{

return 0;

}


node->deleted = 1;


return 1;


}



int file_restore(
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


if(!node->deleted)
{

return 0;

}


node->deleted = 0;


return 1;


}



int file_is_trashed(
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


return node->deleted;


}



u32 file_empty_trash()
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


u32 removed = 0;


/*
    Parcourt TOUS les inodes existants (utilises ou non --
    inode_get() gere l'index hors bornes en renvoyant 0, voir
    inode.c) plutot qu'une liste separee des elements de la
    corbeille : ce systeme de fichiers ne modelise les fichiers
    que comme une liste plate d'inodes (voir la note dans
    filesystem/directory.c), donc pas de structure de repertoire
    "corbeille" dediee a maintenir en plus.
*/

for(u32 i = 1; i <= MAX_INODES; i++)
{


inode* node = inode_get(i);


if(node != 0 && node->used && node->deleted)
{


char name_copy[FILE_NAME_SIZE];

int j = 0;

while(node->name[j] && j < FILE_NAME_SIZE - 1)
{

name_copy[j] = node->name[j];

j++;

}

name_copy[j] = 0;


inode_delete(name_copy);

removed++;


}


}


return removed;


}



int file_rename(
char* old_name,
char* new_name
)
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


if(new_name == 0 || new_name[0] == 0)
{

return 0;

}


inode* node = inode_find(old_name);


if(node == 0)
{

return 0;

}


if(inode_find(new_name) != 0)
{

/* Nom deja utilise (fichier, dossier, ou element de la corbeille). */

return 0;

}


u32 j = 0;

while(new_name[j] && j < FILE_NAME_SIZE - 1)
{

node->name[j] = new_name[j];

j++;

}

node->name[j] = 0;


return 1;


}
