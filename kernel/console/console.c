#include "console.h"

#include "../drivers/framebuffer.h"

#include "../drivers/graphics/graphics.h"

#include "../../gui/theme.h"

#include "../../gui/gui.h"



static int cursor_x=0;
static int cursor_y=0;


/* Voir console_set_top_margin() (console.h). */

static int console_margin_rows = 0;


void console_set_top_margin(int rows)
{

if (rows < 0)
{

rows = 0;

}

console_margin_rows = rows;

if (cursor_y < console_margin_rows)
{

cursor_y = console_margin_rows;

}

}


/*
    Correctif (Terminal/Installateur sans identite visuelle
    propre) : ces deux macros (GFX_FG/GFX_BG, plus bas) suivaient
    jusqu'ici TOUJOURS le theme courant (clair/sombre). Un vrai
    terminal a traditionnellement un fond NOIR quel que soit le
    theme du reste du bureau (voir gui/desktop.c,
    desktop_draw_terminal_frame()) -- ces fonctions permettent
    de le forcer ponctuellement, tout en gardant le theme normal
    partout ailleurs (desactive par defaut).
*/

static int console_fg_override_enabled = 0;

static gfx_color console_fg_override_color = 0;

static int console_bg_override_enabled = 0;

static gfx_color console_bg_override_color = 0;


void console_set_fg_override(gfx_color color, int enabled)
{

console_fg_override_color = color;

console_fg_override_enabled = enabled;

}


void console_set_bg_override(gfx_color color, int enabled)
{

console_bg_override_color = color;

console_bg_override_enabled = enabled;

}


static gfx_color console_current_fg()
{

return console_fg_override_enabled ? console_fg_override_color : theme_text();

}


static gfx_color console_current_bg()
{

return console_bg_override_enabled ? console_bg_override_color : theme_desktop_bg();

}


/*
    ============================================================
    Correctif (etape 2 interface graphique) : bascule pixels
    ============================================================

    Toutes les fonctions ci-dessous verifient gfx_available()
    et delegatent au rendu pixel (police 8x8, voir
    kernel/drivers/graphics) quand un mode graphique reel a ete
    active par boot/loader/stage2.asm. Aucun appelant existant
    (login.c, commands.c, msh.c, kernel.c...) n'a besoin d'etre
    modifie : ils continuent d'appeler console_write() comme
    avant, dans les deux cas.

    GFX_SCALE=2 donne des caracteres 16x16 pixels (police 8x8
    agrandie), plus lisibles sur un ecran haute resolution que
    des caracteres 8x8 natifs minuscules.
*/

#define GFX_SCALE 2

/*
    Anciennement des macros fixes (GFX_CYAN sur GFX_DARKBLUE,
    theme "futuriste"). Deviennent des fonctions vers gui/theme.c
    pour suivre le theme clair/sombre courant (voir
    theme_toggle_dark_mode(), apps/settings/settings.c) au lieu
    d'une seule palette figee au demarrage.
*/

#define GFX_FG (console_current_fg())

#define GFX_BG (console_current_bg())


static u32 gfx_cols()
{

return gfx_width() / (8 * GFX_SCALE);

}


static u32 gfx_rows()
{

return gfx_height() / (8 * GFX_SCALE);

}


void console_init()
{

cursor_x=0;
cursor_y=0;


if (gfx_available())
{

gfx_clear(GFX_BG);

}

}



/*
    Redirection de sortie (voir console_redirect_start()) :
    utilisee par les applications-fenetres qui executent des
    commandes shell (voir apps/terminal/terminal.c,
    apps/installer_app/installer_app.c) pour recuperer le texte
    que execute_command() (shell/commands.c) ecrit normalement
    a l'ecran, et l'afficher elles-memes dans leur PROPRE zone
    de fenetre au lieu d'un console_write() plein ecran --
    aucune des nombreuses commandes shell existantes n'a besoin
    d'etre modifiee, la redirection est totalement transparente
    pour elles.
*/

static char* console_redirect_buffer = 0;

static u32 console_redirect_capacity = 0;

static u32 console_redirect_length = 0;


void console_redirect_start(char* buffer, u32 capacity)
{

console_redirect_buffer = buffer;

console_redirect_capacity = capacity;

console_redirect_length = 0;

if (capacity > 0)
{

buffer[0] = 0;

}

}


void console_redirect_stop()
{

console_redirect_buffer = 0;

}


void console_write(
const char* text
)
{


if (console_redirect_buffer != 0)
{

while (*text && console_redirect_length + 1 < console_redirect_capacity)
{

console_redirect_buffer[console_redirect_length++] = *text;

text++;

}

console_redirect_buffer[console_redirect_length] = 0;

return;

}


if (gfx_available())
{


while(*text)
{


if(*text=='\n')
{

cursor_x=0;
cursor_y++;

text++;

if (cursor_y >= (int)gfx_rows())
{

gfx_scroll_up_region((u32)console_margin_rows * 8 * GFX_SCALE, 8 * GFX_SCALE, GFX_BG);

cursor_y = (int)gfx_rows() - 1;

}

continue;

}


gfx_draw_char(
(u32)cursor_x * 8 * GFX_SCALE,
(u32)cursor_y * 8 * GFX_SCALE,
*text,
GFX_FG,
GFX_SCALE
);


cursor_x++;

text++;


if (cursor_x >= (int)gfx_cols())
{

cursor_x = 0;

cursor_y++;

if (cursor_y >= (int)gfx_rows())
{

gfx_scroll_up_region((u32)console_margin_rows * 8 * GFX_SCALE, 8 * GFX_SCALE, GFX_BG);

cursor_y = (int)gfx_rows() - 1;

}

}


}


return;

}



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
theme_text_attr()
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

if (gfx_available())
{


if (cursor_x == 0)
{

if (cursor_y == 0)
{

return;

}

cursor_y--;

cursor_x = (int)gfx_cols() - 1;

}
else
{

cursor_x--;

}


gfx_fill_rect(
(u32)cursor_x * 8 * GFX_SCALE,
(u32)cursor_y * 8 * GFX_SCALE,
8 * GFX_SCALE,
8 * GFX_SCALE,
GFX_BG
);


return;

}


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
theme_text_attr()
);

}



void console_clear()
{


if (gfx_available())
{

gfx_clear(GFX_BG);


/*
    Correctif (residus visuels du curseur souris) : l'ecran
    entier vient d'etre efface, donc tout ce que
    gui_draw_cursor() avait sauvegarde pour restaurer le fond
    sous le curseur (voir gui/gui.c) ne correspond plus a
    rien -- sans cette ligne, le PREMIER appel a
    gui_draw_cursor() apres un console_clear() repeindrait un
    bloc de pixels perimes ("fantome" a l'ancienne position du
    curseur, ex. en passant d'une application GUI a une autre).
*/

gui_cursor_reset();

cursor_x = 0;

cursor_y = console_margin_rows;

return;

}


fb_clear();

cursor_x = 0;

cursor_y = 0;

}
