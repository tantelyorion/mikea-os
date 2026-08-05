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

    /*
        Correctif securite/stabilite : fb_put() ecrivait
        directement dans "framebuffer[y*VGA_WIDTH+x]" sans
        jamais verifier que (x, y) restait dans la grille
        80x25. Le tableau "framebuffer" pointe sur une zone
        fixe de 4000 octets (0xB8000) : le moindre appel avec
        x >= 80 ou y >= 25 (ou negatif) ecrivait donc en
        dehors de la memoire video, dans de la memoire noyau
        arbitraire. C'est exactement ce qui se produisait au
        demarrage : kernel_start() affiche plus de 25 lignes
        via console_write(), et rien ne limitait jusque-la
        cursor_y.
    */

    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT)
    {

        return;

    }

    framebuffer[y*VGA_WIDTH+x]
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



void fb_scroll()
{

/*
    Deplace chaque ligne 1..VGA_HEIGHT-1 vers la ligne du
    dessus, puis vide la derniere ligne. Utilise par
    console.c quand le curseur atteint le bas de l'ecran,
    pour donner un vrai defilement au lieu de laisser
    cursor_y grandir indefiniment (ce que fb_put() rejette
    desormais silencieusement).
*/

for (int y = 1; y < VGA_HEIGHT; y++)
{

for (int x = 0; x < VGA_WIDTH; x++)
{

framebuffer[(y-1)*VGA_WIDTH+x] = framebuffer[y*VGA_WIDTH+x];

}

}


for (int x = 0; x < VGA_WIDTH; x++)
{

framebuffer[(VGA_HEIGHT-1)*VGA_WIDTH+x] = (0x00 << 8) | ' ';

}

}