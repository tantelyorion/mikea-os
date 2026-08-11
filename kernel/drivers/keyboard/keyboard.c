#include "keyboard.h"

#include "../../cpu/io.h"


#define KEYBOARD_DATA_PORT 0x60

#define KEYBOARD_BUFFER_SIZE 128


static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];

static int buffer_position = 0;


static int shift_pressed = 0;


/*
    Table de correspondance scancode (set 1, touche
    enfoncee) -> ASCII, disposition US QWERTY. Volontairement
    limitee aux touches les plus utiles a un shell (lettres,
    chiffres, ponctuation courante, espace, entree, retour
    arriere) : les touches non couvertes (F1-F12, fleches,
    touches mortes...) sont simplement ignorees pour l'instant.
*/

static const char SCANCODE_ASCII[59] =
{

0,   27,  '1', '2', '3', '4', '5', '6', '7', '8',
'9', '0', '-', '=', '\b', '\t',
'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
'[', ']', '\n', 0,
'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
';', '\'', '`', 0, '\\',
'z', 'x', 'c', 'v', 'b', 'n', 'm',
',', '.', '/', 0,
'*', 0, ' '

};


static const char SCANCODE_ASCII_SHIFT[59] =
{

0,   27,  '!', '@', '#', '$', '%', '^', '&', '*',
'(', ')', '_', '+', '\b', '\t',
'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
'{', '}', '\n', 0,
'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
':', '"', '~', 0, '|',
'Z', 'X', 'C', 'V', 'B', 'N', 'M',
'<', '>', '?', 0,
'*', 0, ' '

};


#define SCANCODE_LEFT_SHIFT  0x2A
#define SCANCODE_RIGHT_SHIFT 0x36

/* Bit 7 pose = relachement de la touche (scancode set 1). */
#define SCANCODE_RELEASE_MASK 0x80



void keyboard_init()
{

buffer_position = 0;

shift_pressed = 0;

}



int keyboard_available()
{

return buffer_position > 0;

}



/*
    Correctif (identifiants toujours refuses au premier essai) :
    rien ne videait le tampon circulaire avant que login_prompt()
    (security/login.c) ne commence a lire le nom d'utilisateur.
    Toute touche pressee pendant le demarrage (l'utilisateur tape
    souvent au clavier par impatience pendant que les messages de
    demarrage defilent) restait donc en attente dans ce tampon et
    etait silencieusement consommee comme si elle faisait partie
    du nom d'utilisateur ou du mot de passe reellement saisis
    ensuite -- l'identifiant semblait alors toujours incorrect,
    meme en tapant "root"/"mikea" correctement.
*/

void keyboard_flush()
{

buffer_position = 0;

}



char keyboard_getchar()
{

if(buffer_position == 0)
{

return 0;

}


char c;


c = keyboard_buffer[0];


for(int i=0;i<KEYBOARD_BUFFER_SIZE-1;i++)
{

keyboard_buffer[i]=keyboard_buffer[i+1];

}


buffer_position--;


return c;

}



static void keyboard_push(char c)
{

if (c == 0)
{
return;
}


if (buffer_position >= KEYBOARD_BUFFER_SIZE)
{

/* Tampon plein : on ignore la frappe plutot que de deborder. */

return;

}


keyboard_buffer[buffer_position] = c;

buffer_position++;

}



void keyboard_handle_irq()
{

/*
    Avant ce fichier, rien ne lisait jamais le port 0x60 :
    keyboard_getchar() renvoyait toujours 0 et le shell ne
    pouvait recevoir aucune frappe reelle au clavier.
*/

u8 scancode = inb(KEYBOARD_DATA_PORT);


if (scancode == SCANCODE_LEFT_SHIFT || scancode == SCANCODE_RIGHT_SHIFT)
{

shift_pressed = 1;

return;

}


if (scancode == (SCANCODE_LEFT_SHIFT | SCANCODE_RELEASE_MASK) ||
    scancode == (SCANCODE_RIGHT_SHIFT | SCANCODE_RELEASE_MASK))
{

shift_pressed = 0;

return;

}


if (scancode & SCANCODE_RELEASE_MASK)
{

/* Relachement d'une autre touche : rien a faire. */

return;

}


if (scancode >= 59)
{

/* Touche non couverte par la table (F1-F12, fleches...). */

return;

}


char c = shift_pressed ? SCANCODE_ASCII_SHIFT[scancode] : SCANCODE_ASCII[scancode];


keyboard_push(c);

}
