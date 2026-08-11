/* Mock du pilote VGA reel (kernel/drivers/framebuffer.c) pour
   pouvoir executer gui.c / console.c tels quels sur la machine
   hote (terminal) au lieu du materiel VGA reel 0xB8000, afin
   de demontrer leur fonctionnement sans avoir a booter un vrai
   noyau bare-metal. */
#include "framebuffer.h"
#include <stdio.h>

static char cell_char[FB_HEIGHT][FB_WIDTH];
static unsigned char cell_color[FB_HEIGHT][FB_WIDTH];

void fb_clear() {
    for (int y = 0; y < FB_HEIGHT; y++)
        for (int x = 0; x < FB_WIDTH; x++) {
            cell_char[y][x] = ' ';
            cell_color[y][x] = 0x00;
        }
}

void fb_put(int x, int y, char c, u8 color) {
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    cell_char[y][x] = c;
    cell_color[y][x] = color;
}

void fb_write(const char* text, int x, int y) {
    int i = 0;
    while (text[i]) { fb_put(x + i, y, text[i], 0x0F); i++; }
}

void fb_scroll() {
    for (int y = 0; y < FB_HEIGHT - 1; y++)
        for (int x = 0; x < FB_WIDTH; x++) {
            cell_char[y][x] = cell_char[y+1][x];
            cell_color[y][x] = cell_color[y+1][x];
        }
    for (int x = 0; x < FB_WIDTH; x++) {
        cell_char[FB_HEIGHT-1][x] = ' ';
        cell_color[FB_HEIGHT-1][x] = 0x00;
    }
}

/* Rendu terminal : couleurs VGA -> ANSI, avec bordure pour
   simuler l'ecran 80x25. */
static int ansi_fg(u8 vga) {
    static const int map[16] = {30,34,32,36,31,35,33,37,90,94,92,96,91,95,93,97};
    return map[vga & 0x0F];
}
static int ansi_bg(u8 vga) {
    static const int map[16] = {40,44,42,46,41,45,43,47,100,104,102,106,101,105,103,107};
    return map[(vga >> 4) & 0x0F];
}

void fb_render_to_terminal() {
    printf("+");
    for (int x = 0; x < FB_WIDTH; x++) printf("-");
    printf("+\n");
    for (int y = 0; y < FB_HEIGHT; y++) {
        printf("|");
        for (int x = 0; x < FB_WIDTH; x++) {
            u8 c = cell_color[y][x];
            printf("\033[%d;%dm%c\033[0m", ansi_fg(c), ansi_bg(c), cell_char[y][x]);
        }
        printf("|\n");
    }
    printf("+");
    for (int x = 0; x < FB_WIDTH; x++) printf("-");
    printf("+\n");
}
