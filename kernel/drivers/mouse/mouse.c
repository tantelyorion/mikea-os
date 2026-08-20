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


/*
    Correctif (curseur "hors de controle", impossible de
    cliquer un petit bouton) : la version precedente amplifiait
    TOUJOURS le mouvement (x2 en usage normal, x4 des qu'il
    depassait un tres petit seuil) EN PLUS de la resolution
    materielle deja doublee dans mouse_init() (0xE8, parametre
    3) -- l'empilement des deux faisait qu'un mouvement de
    souris normal, precis, se traduisait par un saut de
    curseur bien trop grand pour viser un petit bouton (ex.
    touches de la calculatrice) : le curseur "bougeait" bien
    (d'ou l'impression que tout fonctionnait), mais toujours
    trop loin pour cliquer dessus -- exactement l'inverse de ce
    qui etait recherche.

    La resolution materielle augmentee suffit deja a corriger
    la lenteur d'origine ; on ne rajoute plus ici qu'une TRES
    legere acceleration, reservee aux mouvements clairement
    rapides (grand geste pour traverser l'ecran, magnitude
    largement au-dela de ce qu'un geste de visee produit) --
    dans le meme esprit que "Ameliorer la precision du
    pointeur" de Windows ou l'acceleration de macOS : precis a
    vitesse normale, plus rapide seulement pour les grands
    gestes.
*/

#define MOUSE_BASE_SENSITIVITY 1

#define MOUSE_FAST_THRESHOLD   20

#define MOUSE_FAST_SENSITIVITY 2


static s32 mouse_apply_sensitivity(s32 delta)
{

s32 magnitude = (delta < 0) ? -delta : delta;

s32 factor = (magnitude >= MOUSE_FAST_THRESHOLD)
? MOUSE_FAST_SENSITIVITY
: MOUSE_BASE_SENSITIVITY;

return delta * factor;

}


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


/*
    Correctif (curseur "dur a deplacer") : "set defaults"
    (0xF6, ci-dessus) remet la souris a sa resolution ET son
    taux d'echantillonnage par defaut -- respectivement 4
    comptes/mm et 100 rapports/seconde pour un PS/2 standard.
    C'est deux fois moins fin et deux fois moins reactif que ce
    dont ce controleur est capable, ce qui donnait l'impression
    d'un curseur "englue" (il fallait deplacer la souris
    physique sur une grande distance pour un petit deplacement
    a l'ecran, avec un mouvement saccade). On demande ici
    explicitement le maximum standard : resolution 8 comptes/mm
    (0xE8, parametre 3) et 200 rapports/seconde (0xF3, parametre
    200) -- valeurs prises en charge par tout controleur PS/2
    standard, y compris ceux emules par QEMU/VirtualBox.
    Non bloquant : en cas d'echec (accuse de reception absent),
    on continue avec les valeurs par defaut plutot que d'arreter
    toute l'initialisation pour un simple confort visuel.
*/

if (mouse_write(0xE8) == 0 && mouse_read(&ack) == 0 && ack == 0xFA)
{

mouse_write(3);

mouse_read(&ack);

}


if (mouse_write(0xF3) == 0 && mouse_read(&ack) == 0 && ack == 0xFA)
{

mouse_write(200);

mouse_read(&ack);

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


/*
    Correctif CRITIQUE (curseur qui se bloque / "parasite"
    apres un moment d'utilisation) : la resynchronisation se
    faisait auparavant APRES avoir rempli les 3 octets du
    paquet -- en cas d'octet manquant ou en trop (n'importe
    quelle irregularite d'IRQ suffit a le declencher), les 3
    octets etaient jetes d'un bloc et la lecture repartait de
    zero sur l'octet SUIVANT... qui reste decale du meme
    nombre d'octets qu'avant (3 est un multiple de la taille
    du paquet : jeter 3 octets ne corrige jamais un decalage de
    1 ou 2 octets). Le flux restait alors mal aligne
    INDEFINIMENT : chaque "paquet" lu etait en realite un
    melange de la fin d'un vrai paquet et du debut du suivant,
    accepte ou rejete presque au hasard selon que son bit 3
    tombait par hasard a 1 -- exactement ce qui produit les
    deux symptomes decrits (curseur qui semble se figer quand
    le paquet est rejete, sursauts erratiques quand un paquet
    corrompu est accepte par hasard, boutons qui semblent
    "parasites").

    Le correctif verifie le bit 3 DES LE PREMIER octet, avant
    meme de le stocker : un octet errant recu alors qu'on
    attend un DEBUT de paquet est simplement ignore, un par un,
    sans toucher a l'alignement des octets suivants -- meme
    principe de resynchronisation que les pilotes PS/2 serieux
    (ex. psmouse sous Linux). Un decalage d'un ou deux octets se
    corrige ainsi tout seul en au plus deux octets ignores, au
    lieu de rester casse pour le reste de la session.
*/

if (packet_index == 0 && (data & 0x08) == 0)
{

return;

}


packet[packet_index] = data;

packet_index++;


if (packet_index < 3)
{

return;

}


packet_index = 0;


u8 flags = packet[0];


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


dx = mouse_apply_sensitivity(dx);

dy = mouse_apply_sensitivity(dy);


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
