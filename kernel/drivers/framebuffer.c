#include "framebuffer.h"


#define VGA_WIDTH 80
#define VGA_HEIGHT 25


static u16* framebuffer =
(u16*)0xB8000;



void fb_clear()
{

    for(int y=0;y<VGA_HEIGHT;y++)
    {

        for(int x=0;x<VGA_WIDTH;x++)
        {

            framebuffer[y*VGA_WIDTH+x]
            =
            (0x00 << 8) | ' ';

        }

    }

}



void fb_put(
int x,
int y,
char c,
u8 color
)
{

    framebuffer[
    y*VGA_WIDTH+x
    ]
    =
    ((u16)color<<8)|c;

}



void fb_write(
const char* text,
int x,
int y
)
{

int i=0;


while(text[i])
{

fb_put(
x+i,
y,
text[i],
0x0F
);


i++;

}


}