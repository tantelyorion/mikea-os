/*
====================================================

        Mikea OS Filesystem

        Superblock Manager


        MKFS v2


====================================================
*/


/*
    Etat de ce module (pour eviter toute mauvaise
    interpretation par la suite) :

    - Correctif (persistance disque) : superblock_write()/
      superblock_load() sont desormais reellement utilisees
      (voir filesystem/mkfs.c, fs_already_formatted()) --
      elles servent en particulier a distinguer un disque de
      donnees deja forme (on recharge) d'un disque vierge (on
      formate), et le superbloc est reecrit a chaque
      modification du bitmap des blocs (voir
      block_table_save(), filesystem/block.c) pour rester
      coherent avec free_blocks. Ce branchement attendait la
      persistance de la table des inodes elle-meme (voir
      inode_table_save()/load(), filesystem/inode.c) --
      desormais fait, voir fs_layout.h pour la disposition
      disque complete.

    - free_blocks et free_inodes sont maintenant mis a jour a
      chaque appel de block_allocate()/block_free()
      (filesystem/block.c) et inode_create_ex()/inode_delete()
      (filesystem/inode.c) -- ils refletent l'etat reel et
      peuvent etre exposes (ex. une future commande shell
      "df"/"stat") sans donner une fausse image du systeme de
      fichiers.
*/


#include "superblock.h"

#include "disk.h"

#include "block.h"

#include "fs_layout.h"





/*
====================================================
GLOBAL SUPERBLOCK

Chargé en mémoire kernel

====================================================
*/


static superblock main_superblock;






/*
====================================================
INITIALIZE SUPERBLOCK
====================================================
*/


void superblock_init()
{


/*
Magic identifier
*/


main_superblock.magic[0]='M';

main_superblock.magic[1]='I';

main_superblock.magic[2]='K';

main_superblock.magic[3]='E';

main_superblock.magic[4]='A';

main_superblock.magic[5]=0;



/*
Version MKFS

*/

main_superblock.version = 2;




/*
Disk information

*/

main_superblock.block_size = 512;


main_superblock.total_blocks = 2048;


/*
    Correctif (persistance disque, voir fs_layout.h) : ce
    n'est plus seulement le bloc 0 qui est reserve, mais tous
    les blocs 0 a FS_FIRST_DATA_BLOCK-1 (superbloc + table des
    inodes + bitmap des blocs) -- 2020 blocs de donnees
    reellement disponibles desormais, et non 2047.
*/

main_superblock.free_blocks = TOTAL_BLOCKS - FS_FIRST_DATA_BLOCK;




/*
Inodes

*/

main_superblock.total_inodes = 128;


main_superblock.free_inodes = 128;




/*
Filesystem state

*/

main_superblock.mounted = 1;



}







/*
====================================================
WRITE SUPERBLOCK TO DISK

Block 0 réservé

====================================================
*/


void superblock_write()
{


u8 buffer[512];



/*
Clear buffer

*/

for(
int i=0;
i<512;
i++
)
{


buffer[i]=0;


}



/*
Copy structure

*/

u8* source;


source =
(u8*)&main_superblock;



for(
u32 i=0;
i<sizeof(superblock);
i++
)
{


buffer[i]=source[i];


}



/*
Write first block

*/

block_write(
0,
buffer
);



}







/*
====================================================
LOAD SUPERBLOCK

====================================================
*/


void superblock_load()
{


u8 buffer[512];



block_read(
0,
buffer
);



u8* destination;


destination =
(u8*)&main_superblock;



for(
u32 i=0;
i<sizeof(superblock);
i++
)
{


destination[i]=buffer[i];


}



}







/*
====================================================
GET SUPERBLOCK
====================================================
*/


superblock* superblock_get()
{


return &main_superblock;


}