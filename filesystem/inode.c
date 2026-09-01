/*
====================================================

        Mikea OS Filesystem

        Inode Manager


        MKFS v2


====================================================
*/


#include "inode.h"

#include "block.h"

#include "disk.h"

#include "fs_layout.h"

#include "superblock.h"





/*
====================================================
INODE TABLE

Correctif (persistance disque) : desormais sauvegardee/
rechargee sur le vrai disque a chaque modification -- voir
inode_table_save()/inode_table_load() plus bas, et fs_layout.h
pour l'emplacement reserve. Reste un tableau en RAM au
quotidien (toutes les fonctions ci-dessous continuent d'operer
directement dessus, sans latence disque a chaque lecture) :
seule la PERSISTANCE entre deux demarrages a change.

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


/* Correctif (fichiers multi-blocs) : toujours reinitialiser -- un inode reutilise peut porter la valeur laissee par son ancien occupant. */

node->indirect_block = 0;


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


/*
    Correctif (compteur superbloc + persistance disque, voir
    fs_layout.h) : meme raisonnement que block_allocate()
    (filesystem/block.c) -- free_inodes etait fige, jamais mis
    a jour ; inode_table_save() rend la creation persistante
    immediatement (pas seulement le bloc de donnees deja
    alloue plus haut).
*/

superblock* sb = superblock_get();

if(sb->free_inodes > 0)
{

sb->free_inodes--;

}


inode_table_save();



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


/*
    Correctif (fichiers multi-blocs, voir inode.h) : libere
    aussi le bloc indirect et TOUS les blocs de donnees qu'il
    reference -- sans quoi la suppression d'un fichier de plus
    de 512 octets ne liberait que son premier bloc, le reste
    restant marque "utilise" pour toujours (fuite d'espace
    disque permanente).
*/

if (node->indirect_block != 0)
{

u8 indirect_buffer[BLOCK_SIZE];

block_read(node->indirect_block, indirect_buffer);


u32* block_list = (u32*)indirect_buffer;

u32 max_entries = BLOCK_SIZE / sizeof(u32);


for (u32 i = 0; i < max_entries; i++)
{

if (block_list[i] != 0)
{

block_free(block_list[i]);

}

}


block_free(node->indirect_block);

}




node->used=0;


node->id=0;


node->size=0;


node->block=0;


node->indirect_block=0;


node->deleted=0;


node->is_directory=0;


node->parent[0]=0;



current_inode_count--;


/* Correctif (compteur superbloc + persistance disque) : voir inode_create_ex() ci-dessus. */

superblock* sb = superblock_get();

sb->free_inodes++;


inode_table_save();



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



/*
====================================================
SAVE / LOAD INODE TABLE (persistance disque)

Voir fs_layout.h (FS_INODE_TABLE_START_BLOCK) et mkfs.c.
Ecrit/relit directement le tableau brut (128 * sizeof(inode) =
EXACTEMENT FS_INODE_TABLE_BLOCKS blocs de 512 octets, voir
fs_layout.h) via disk_write_buffer()/disk_read_buffer() -- pas
via block_write()/block_read(), qui operent sur des blocs de
DONNEES (indices 0-2047 dans l'espace des blocs), alors que
cette zone est une zone de METADONNEES a part, adressee
directement en octets.
Sauvegarde aussi le superbloc (superblock_write()) au passage,
meme raisonnement que block_table_save() (filesystem/block.c)
pour free_blocks : sans cela, free_inodes (mis a jour juste
avant cet appel, dans inode_create_ex()/inode_delete()) ne
serait persiste sur disque qu'au prochain appel de
block_table_save() -- decale d'un cycle de creation/suppression
par rapport a la valeur reelle en RAM.
====================================================
*/


void inode_table_save()
{


disk_write_buffer(
FS_INODE_TABLE_START_BLOCK * BLOCK_SIZE,
(u8*)inode_table,
sizeof(inode_table)
);


superblock_write();


}



void inode_table_load()
{


disk_read_buffer(
FS_INODE_TABLE_START_BLOCK * BLOCK_SIZE,
(u8*)inode_table,
sizeof(inode_table)
);


/*
    current_inode_count n'est pas serialise avec inode_table
    (compteur derive, pas une information supplementaire) --
    recalcule ici par un simple comptage, pour rester coherent
    avec les entrees "used" qui viennent d'etre rechargees.
*/

current_inode_count = 0;

for(
u32 i=0;
i<MAX_INODES;
i++
)
{

if(inode_table[i].used)
{

current_inode_count++;

}

}


}
