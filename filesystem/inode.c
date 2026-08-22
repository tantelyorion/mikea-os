/*
====================================================

        Mikea OS Filesystem

        Inode Manager


        MKFS v2


====================================================
*/


#include "inode.h"

#include "block.h"





/*
====================================================
INODE TABLE

En mémoire pour l'instant.

Sera sauvegardée sur disque ensuite.


====================================================
*/


static inode inode_table[MAX_INODES];



static u32 current_inode_count = 0;







/*
====================================================
INITIALIZE INODES
====================================================
*/


void inode_init()
{


for(
int i=0;
i<MAX_INODES;
i++
)
{


inode_table[i].id = 0;


inode_table[i].used = 0;


inode_table[i].size = 0;


inode_table[i].block = 0;


inode_table[i].permission = 0;


inode_table[i].deleted = 0;


inode_table[i].is_directory = 0;


inode_table[i].parent[0] = 0;


}



current_inode_count = 0;



}







/*
====================================================
CREATE INODE

====================================================
*/


inode* inode_create(
char* name
)
{

return inode_create_ex(name, "", 0);

}



inode* inode_create_ex(
char* name,
char* parent,
int is_directory
)
{


/*
Check existing file

*/

inode* existing;



existing =
inode_find(name);



if(existing != 0)
{

return existing;

}






/*
Find free inode

*/

for(
u32 i=0;
i<MAX_INODES;
i++
)
{


if(inode_table[i].used==0)
{


inode* node;



node =
&inode_table[i];



/*
Allocate data block

Correctif : le resultat de block_allocate() n'etait
jamais verifie. Quand le disque etait plein (aucun
bloc libre, block_allocate() renvoie 0), l'inode
etait quand meme cree et marque "used" avec block=0
(le bloc reserve/sentinelle), ce qui corrompait
silencieusement le fichier au lieu de signaler un
echec.

*/

u32 new_block =
block_allocate();

if(new_block == 0)
{

return 0;

}



node->id=i+1;


node->used=1;


node->size=0;


node->block =
new_block;


node->permission =
7;


/*
    Corbeille (voir filesystem/file.c, file_trash()) :
    remis explicitement a 0 a la creation -- necessaire
    depuis que inode_create() peut reutiliser une entree
    precedemment liberee par inode_delete() (voir plus bas
    dans ce fichier, "used=0" mais rien ne remettait
    "deleted" a 0 auparavant, ce champ n'existant pas encore).
*/

node->deleted =
0;


node->is_directory =
is_directory;


u32 p = 0;

while(
parent != 0
&&
parent[p]
&&
p < FILE_NAME_SIZE - 1
)
{

node->parent[p] = parent[p];

p++;

}

node->parent[p] = 0;




/*
Copy name

*/

u32 j=0;



while(
name[j]
&&
j<FILE_NAME_SIZE-1
)
{


node->name[j]=name[j];


j++;


}



node->name[j]=0;



current_inode_count++;



return node;


}


}




return 0;


}








/*
====================================================
FIND INODE BY NAME

====================================================
*/


inode* inode_find(
char* name
)
{


for(
u32 i=0;
i<MAX_INODES;
i++
)
{


if(
inode_table[i].used
)
{


int same=1;



u32 j=0;



while(
name[j]
||
inode_table[i].name[j]
)
{


if(
name[j]
!=
inode_table[i].name[j]
)
{


same=0;


break;


}


j++;


}



if(same)
{

return &inode_table[i];

}



}


}



return 0;


}







/*
====================================================
DELETE INODE
====================================================
*/


void inode_delete(
char* name
)
{


inode* node;



node =
inode_find(name);



if(node==0)
{

return;

}




/*
Release block

*/

block_free(
node->block
);




node->used=0;


node->id=0;


node->size=0;


node->block=0;


node->deleted=0;


node->is_directory=0;


node->parent[0]=0;



current_inode_count--;



}








/*
====================================================
GET INODE BY ID
====================================================
*/


inode* inode_get(
u32 id
)
{


if(id==0 || id>MAX_INODES)
{

return 0;

}



if(
inode_table[id-1].used
)
{


return &inode_table[id-1];


}



return 0;


}







/*
====================================================
COUNT

====================================================
*/


u32 inode_count()
{


return current_inode_count;


}