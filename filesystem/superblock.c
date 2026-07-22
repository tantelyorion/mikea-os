/*
====================================================

        Mikea OS Filesystem

        Superblock Manager


        MKFS v2


====================================================
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