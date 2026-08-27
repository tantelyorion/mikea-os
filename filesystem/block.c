/*
====================================================

        Mikea OS Filesystem

        Block Manager


        Version:
        MKFS v2


        Developer:
        Tantely Orion


====================================================
*/


#include "block.h"

#include "disk.h"

#include "fs_layout.h"

#include "superblock.h"



/*
====================================================
BLOCK TABLE

0 = libre
1 = utilisé

====================================================
*/


static u8 block_table[TOTAL_BLOCKS];





/*
====================================================
INITIALIZE BLOCK SYSTEM
====================================================
*/


void block_init()
{


for(
u32 i = 0;
i < TOTAL_BLOCKS;
i++
)
{


block_table[i]=0;


}



}





/*
====================================================
CHECK BLOCK FREE
====================================================
*/


int block_is_free(
u32 block
)
{


if(block >= TOTAL_BLOCKS)
{

return 0;

}



if(block_table[block]==0)
{

return 1;

}



return 0;


}







/*
====================================================
ALLOCATE BLOCK

Cherche un bloc libre

Correctif (persistance disque, voir fs_layout.h) : la
recherche commence desormais a FS_FIRST_DATA_BLOCK (28) et non
plus 1 -- les blocs 0 a 27 sont reserves aux metadonnees du
systeme de fichiers lui-meme (superbloc, table des inodes,
bitmap des blocs), jamais a des donnees de fichiers.

====================================================
*/


u32 block_allocate()
{


for(
u32 i = FS_FIRST_DATA_BLOCK;
i < TOTAL_BLOCKS;
i++
)
{


if(block_table[i]==0)
{


block_table[i]=1;


/*
    Correctif (compteur superbloc, voir le commentaire
    d'etat en tete de superblock.c) : free_blocks
    refletait jusqu'ici une valeur figee, jamais mise a
    jour. superblock_get() renvoie un pointeur direct
    vers la structure en memoire -- pas besoin de la
    recharger/reecrire ici, seulement de tenir son
    compteur a jour ; superblock_write() (appele par
    block_table_save() ci-dessous) se charge de le
    rendre persistant.
*/

superblock* sb = superblock_get();

if(sb->free_blocks > 0)
{

sb->free_blocks--;

}


block_table_save();


return i;


}


}



return 0;


}






/*
====================================================
FREE BLOCK
====================================================
*/


void block_free(
u32 block
)
{


if(block >= TOTAL_BLOCKS)
{

return;

}



/*
    Correctif (compteur superbloc) : n'incremente
    free_blocks que si ce bloc etait reellement marque
    "utilise" -- une double liberation ne doit pas gonfler
    artificiellement le compteur.
*/

if(block_table[block] == 1)
{

superblock* sb = superblock_get();

sb->free_blocks++;

}


block_table[block]=0;


block_table_save();



}





/*
====================================================
WRITE BLOCK

512 bytes

====================================================
*/


void block_write(
u32 block,
u8* data
)
{


if(block >= TOTAL_BLOCKS)
{

return;

}



u32 address;



address =
block * BLOCK_SIZE;



disk_write_buffer(
address,
data,
BLOCK_SIZE
);



}






/*
====================================================
READ BLOCK

512 bytes

====================================================
*/


void block_read(
u32 block,
u8* buffer
)
{


if(block >= TOTAL_BLOCKS)
{

return;

}



u32 address;



address =
block * BLOCK_SIZE;



disk_read_buffer(
address,
buffer,
BLOCK_SIZE
);



}



/*
====================================================
SAVE / LOAD BLOCK TABLE (persistance disque)

Voir fs_layout.h (FS_BLOCK_BITMAP_START_BLOCK) et mkfs.c.
Ecrit/relit directement le tableau brut (2048 octets = EXACTEMENT
FS_BLOCK_BITMAP_BLOCKS blocs de 512 octets, voir fs_layout.h) via
disk_write_buffer()/disk_read_buffer() -- pas via block_write()/
block_read() ci-dessus, qui operent sur des blocs de DONNEES
(indices 0-2047 dans l'espace des blocs), alors que cette zone est
une zone de METADONNEES a part, adressee directement en octets.

Sauvegarde aussi le superbloc (superblock_write()) au passage :
free_blocks (mis a jour par block_allocate()/block_free()
ci-dessus) fait partie du meme etat coherent que le bitmap --
les persister ensemble evite un superbloc et un bitmap
desynchronises si le systeme s'arrete brutalement entre les deux.
====================================================
*/


void block_table_save()
{


disk_write_buffer(
FS_BLOCK_BITMAP_START_BLOCK * BLOCK_SIZE,
block_table,
sizeof(block_table)
);


superblock_write();


}



void block_table_load()
{


disk_read_buffer(
FS_BLOCK_BITMAP_START_BLOCK * BLOCK_SIZE,
block_table,
sizeof(block_table)
);


}
