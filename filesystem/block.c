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

====================================================
*/


u32 block_allocate()
{


for(
u32 i = 1;
i < TOTAL_BLOCKS;
i++
)
{


if(block_table[i]==0)
{


block_table[i]=1;


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



block_table[block]=0;



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