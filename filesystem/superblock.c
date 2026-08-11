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

    - superblock_init()/superblock_write()/superblock_load()/
      superblock_get() sont completement implementees et
      fonctionnelles, mais AUCUNE n'est appelee au-dela de
      superblock_init() (voir filesystem/mkfs.c). Ce n'est pas
      un oubli isole : comme le documente deja
      filesystem/inode.c ("INODE TABLE -- En memoire pour
      l'instant. Sera sauvegardee sur disque ensuite."), la
      table des inodes elle-meme n'est pas persistee. Appeler
      superblock_write() seule, sans les inodes qu'elle
      decrit, donnerait une fausse impression de persistance
      au redemarrage (le superblock survivrait, pas les
      fichiers). Le branchement complet de ce module attend
      donc ce travail de persistance des inodes, deja identifie
      comme travail futur.

    - free_blocks (2047) et free_inodes (128) sont des valeurs
      FIXES ecrites une seule fois par superblock_init() : rien
      ne les met a jour quand block_allocate()/block_free()
      (filesystem/block.c) ou inode_create()/inode_delete()
      (filesystem/inode.c) changent l'etat reel du systeme de
      fichiers. Ne pas exposer ces deux champs (ex. via une
      future commande shell "df"/"stat") tant qu'ils ne
      refletent pas l'etat reel -- ce serait un affichage
      fonctionnel mais faux, pire qu'une commande absente.
*/


#include "superblock.h"

#include "disk.h"

#include "block.h"





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


main_superblock.free_blocks = 2047;




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