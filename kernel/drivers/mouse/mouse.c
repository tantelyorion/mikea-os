#include "mouse.h"

#include "../../cpu/io.h"

#include "../../interrupt/pic.h"

#include "../graphics/graphics.h"


#define MOUSE_STATUS_PORT 0x64

#define MOUSE_COMMAND_PORT 0x64

#define MOUSE_DATA_PORT 0x60


/*
    Correctif stabilite (coherent avec ata_wait_ready(), voir
    filesystem/disk.c, et la boucle de recherche de mode VBE de
    stage2.asm) : toutes les attentes de ce fichier sont bornees
    -- un controleur PS/2 defaillant ou absent ne doit jamais
    figer indefiniment le demarrage.
*/

#define MOUSE_TIMEOUT_ITERATIONS 100000


static s32 mouse_x = 0;

static s32 mouse_y = 0;


static int btn_left = 0;

static int btn_right = 0;

static int btn_middle = 0;


static u8 packet[3];

static int packet_index = 0;


static int mouse_wait_input_clear()
{

for (u32 tries = 0; tries < MOUSE_TIMEOUT_ITERATIONS; tries++)
{

if ((inb(MOUSE_STATUS_PORT) & 0x02) == 0)
{

return 0;

}

}

return -1;

}


static int mouse_wait_output_full()
{

for (u32 tries = 0; tries < MOUSE_TIMEOUT_ITERATIONS; tries++)
{

if (inb(MOUSE_STATUS_PORT) & 0x01)
{

return 0;

}

}

return -1;

}


static int mouse_write(u8 value)
{

if (mouse_wait_input_clear() != 0)
{

return -1;

}

outb(MOUSE_COMMAND_PORT, 0xD4);


if (mouse_wait_input_clear() != 0)
{

return -1;

}

outb(MOUSE_DATA_PORT, value);


return 0;

}


static int mouse_read(u8* out)
{

if (mouse_wait_output_full() != 0)
{

return -1;

}

*out = inb(MOUSE_DATA_PORT);

return 0;

}


/*
    Statut de la derniere initialisation, consultable via la
    commande shell "mouse" -- utile si le message de demarrage
    a defile hors ecran avant d'etre lu (meme limitation que le
    message [graphics], voir kernel/drivers/graphics/graphics.c
    et shell/commands.c::cmd_gfxstatus()).
*/

static const char* mouse_init_status = "non initialise";


const char* mouse_get_init_status()
{

return mouse_init_status;

}


void console_write(const char* text);


void mouse_init()
{


mouse_x = 0;

mouse_y = 0;

packet_index = 0;


/* Active le second port PS/2 (souris). */

if (mouse_wait_input_clear() != 0)
{

mouse_init_status = "Timeout (etape 1/6 : avant 0xA8)";

console_write("[mouse] Timeout (etape 1/6 : avant 0xA8)\n");

return;

}

outb(MOUSE_COMMAND_PORT, 0xA8);


/* Lit l'octet de configuration du controleur. */

if (mouse_wait_input_clear() != 0)
{

mouse_init_status = "Timeout (etape 2/6 : avant 0x20)";

console_write("[mouse] Timeout (etape 2/6 : avant 0x20)\n");

return;

}

outb(MOUSE_COMMAND_PORT, 0x20);


u8 status;

if (mouse_read(&status) != 0)
{

mouse_init_status = "Timeout (etape 3/6 : lecture config)";

console_write("[mouse] Timeout (etape 3/6 : lecture config)\n");

return;

}


/*
    Bit 1 = active l'IRQ12 (interruption souris).
    Bit 5 = active l'horloge du second port (0 = active).
*/

status = (u8)((status | 0x02) & ~0x20);


if (mouse_wait_input_clear() != 0)
{

mouse_init_status = "Timeout (etape 4/6 : avant 0x60)";

console_write("[mouse] Timeout (etape 4/6 : avant 0x60)\n");

return;

}

outb(MOUSE_COMMAND_PORT, 0x60);


if (mouse_wait_input_clear() != 0)
{

mouse_init_status = "Timeout (etape 4/6 : ecriture config)";

console_write("[mouse] Timeout (etape 4/6 : ecriture config)\n");

return;

}

outb(MOUSE_DATA_PORT, status);


/* Reglages par defaut, puis active le rapport de mouvement. */

u8 ack;

if (mouse_write(0xF6) != 0 || mouse_read(&ack) != 0)
{

mouse_init_status = "Timeout (etape 5/6 : set defaults 0xF6)";

console_write("[mouse] Timeout (etape 5/6 : set defaults 0xF6)\n");

return;

}

if (ack != 0xFA)
{

mouse_init_status = "Pas d'accuse de reception a 0xF6 (etape 5/6)";

console_write("[mouse] Pas d'accuse de reception a 0xF6 (etape 5/6)\n");

return;

}


if (mouse_write(0xF4) != 0 || mouse_read(&ack) != 0)
{

mouse_init_status = "Timeout (etape 6/6 : activation rapport 0xF4)";

console_write("[mouse] Timeout (etape 6/6 : activation rapport 0xF4)\n");

return;

}

if (ack != 0xFA)
{

mouse_init_status = "Pas d'accuse de reception a 0xF4 (etape 6/6)";

console_write("[mouse] Pas d'accuse de reception a 0xF4 (etape 6/6)\n");

return;

}


/*
    Demasque l'IRQ12 explicitement : voir le commentaire de
    pic_unmask_irq() (kernel/interrupt/pic.c), rien ne
    garantit qu'elle soit deja demasquee par defaut.
*/

pic_unmask_irq(12);


mouse_init_status = "Initialisation reussie (IRQ12 demasquee)";

console_write("[mouse] Initialisation reussie (IRQ12 demasquee)\n");


}


void mouse_handle_irq()
{


u8 data = inb(MOUSE_DATA_PORT);


packet[packet_index] = data;

packet_index++;


if (packet_index < 3)
{

return;

}


packet_index = 0;


u8 flags = packet[0];


/*
    Bit 3 doit toujours valoir 1 sur le premier octet d'un
    paquet valide -- sert a se resynchroniser si jamais un
    octet a ete perdu (paquet mal aligne, on l'ignore
    silencieusement plutot que d'interpreter des donnees
    incoherentes).
*/

if ((flags & 0x08) == 0)
{

return;

}


btn_left = flags & 0x01;

btn_right = (flags & 0x02) != 0;

btn_middle = (flags & 0x04) != 0;


s32 dx = (s32)packet[1];

s32 dy = (s32)packet[2];


if (flags & 0x10)
{

dx -= 256;

}


if (flags & 0x20)
{

dy -= 256;

}


mouse_x += dx;

/* L'axe Y du PS/2 est inverse (positif = vers le haut). */

mouse_y -= dy;


if (gfx_available())
{


if (mouse_x < 0)
{

mouse_x = 0;

}


if (mouse_y < 0)
{

mouse_y = 0;

}


if (mouse_x >= (s32)gfx_width())
{

mouse_x = (s32)gfx_width() - 1;

}


if (mouse_y >= (s32)gfx_height())
{

mouse_y = (s32)gfx_height() - 1;

}


}


}


s32 mouse_get_x()
{

return mouse_x;

}


s32 mouse_get_y()
{

return mouse_y;

}


int mouse_left_pressed()
{

return btn_left;

}


int mouse_right_pressed()
{

return btn_right;

}


int mouse_middle_pressed()
{

return btn_middle;

}
