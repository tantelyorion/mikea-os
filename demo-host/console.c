#include "console.h"

#include "framebuffer.h"



static int cursor_x=0;
static int cursor_y=0;



void console_init()
{

cursor_x=0;
cursor_y=0;

}

void console_write_dec(u32 val) {
    if (val == 0) {
        console_write("0");
        return;
    }
    char buf[12];
    int i = 10;
    buf[11] = '\0';
    while (val > 0 && i >= 0) {
        buf[i--] = '0' + (val % 10);
        val /= 10;
    }
    console_write(&buf[i + 1]);
}

void console_write_hex(u16 val) {
    const char hex_chars[] = "0123456789ABCDEF";
    char buf[7];
    buf[0] = '0';
    buf[1] = 'x';
    buf[2] = hex_chars[(val >> 12) & 0x0F];
    buf[3] = hex_chars[(val >> 8) & 0x0F];
    buf[4] = hex_chars[(val >> 4) & 0x0F];
    buf[5] = hex_chars[val & 0x0F];
    buf[6] = '\0';
    console_write(buf);
}

void console_write(
const char* text
)
{


while(*text)
{


if(*text=='\n')
{

cursor_x=0;
cursor_y++;

text++;

if (cursor_y >= FB_HEIGHT)
{

fb_scroll();

cursor_y = FB_HEIGHT - 1;

}

continue;

}


/*
    Correctif : la version precedente appelait
    fb_write() (qui reecrit toute la chaine restante)
    a chaque caractere, soit un travail O(n^2) pour
    une chaine de longueur n. On ecrit ici un seul
    caractere a la fois avec fb_put(), en O(n).
*/

fb_put(
cursor_x,
cursor_y,
*text,
0x0F
);


cursor_x++;

text++;


/*
    Correctif : cursor_x/cursor_y n'etaient jamais
    bornes a la grille VGA (80x25). Avec plus de 25
    lignes affichees (le boot a lui seul en ecrit plus
    de 50), cursor_y depassait largement FB_HEIGHT et
    fb_put() ecrivait hors de la memoire video (voir
    framebuffer.c). On enveloppe desormais la ligne a
    FB_WIDTH et on fait defiler l'ecran a FB_HEIGHT.
*/

if (cursor_x >= FB_WIDTH)
{

cursor_x = 0;

cursor_y++;

if (cursor_y >= FB_HEIGHT)
{

fb_scroll();

cursor_y = FB_HEIGHT - 1;

}

}


}


}



void console_backspace()
{

/*
    Recule le curseur d'une position (y compris en
    remontant d'une ligne si on est en debut de ligne),
    puis efface le caractere qui s'y trouvait.
*/

if (cursor_x == 0)
{

if (cursor_y == 0)
{

/* Deja tout en haut a gauche : rien a effacer. */

return;

}

cursor_y--;

cursor_x = FB_WIDTH - 1;

}
else
{

cursor_x--;

}


fb_put(
cursor_x,
cursor_y,
' ',
0x0F
);

}



void console_clear()
{

fb_clear();

cursor_x = 0;

cursor_y = 0;

}
