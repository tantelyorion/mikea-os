#include "gui.h"

#include "theme.h"

#include "../kernel/drivers/framebuffer.h"

#include "../kernel/drivers/graphics/graphics.h"

#include "../kernel/drivers/mouse/mouse.h"

#include "../libc/string.h"


void keyboard_flush();

int keyboard_available();

char keyboard_getchar();

unsigned long timer_ticks();


/*
    Correctif (fenetre invisible en mode graphique) : ces
    fonctions ecrivaient directement dans la memoire texte VGA
    (0xB8000) via fb_put() -- sans effet une fois qu'un vrai
    mode graphique pixels est actif (voir
    kernel/console/console.c pour le meme correctif applique a
    la console). On bascule ici vers les primitives pixel
    (kernel/drivers/graphics) des que gfx_available() est vrai,
    avec les memes conventions de coordonnees "cellule de texte"
    et le meme facteur d'echelle (GFX_SCALE=2) que console.c,
    pour que "gui" s'aligne visuellement avec le reste de
    l'affichage.

    Refonte visuelle : l'ancien theme "futuriste" (fond bleu
    nuit, texte/bordures cyan neon, accents en "L" façon HUD de
    vaisseau) est remplace par le theme neutre noir/blanc/gris
    de gui/theme.c -- panneaux "verre depoli" (glassmorphisme
    via gfx_fill_rect_blend()), ombre portee douce, bordures
    fines, dans l'esprit macOS/GNOME/Windows/Openbox. Le
    parametre "color" de gui_draw_box()/gui_draw_window() reste
    utilise tel quel en mode texte (palette VGA fixe, pas de
    transparence possible), mais est ignore en mode graphique au
    profit du theme courant (voir theme_is_dark()).
*/

#define GFX_SCALE 2


static void gui_draw_text_gfx(int x, int y, const char* text)
{

gfx_draw_text(
(u32)x * 8 * GFX_SCALE,
(u32)y * 8 * GFX_SCALE,
text,
theme_text(),
GFX_SCALE
);

}


void gui_draw_text(int x, int y, const char* text, u8 color)
{


if (gfx_available())
{

gui_draw_text_gfx(x, y, text);

return;

}


int i = 0;

while (text[i] != 0)
{

fb_put(x + i, y, text[i], color);

i++;

}

}



void gui_draw_box(int x, int y, int width, int height, u8 color)
{

if (width < 2 || height < 2)
{
/* Trop petit pour dessiner une bordure complete. */
return;
}


if (gfx_available())
{


u32 px = (u32)x * 8 * GFX_SCALE;

u32 py = (u32)y * 8 * GFX_SCALE;

u32 pw = (u32)width * 8 * GFX_SCALE;

u32 ph = (u32)height * 8 * GFX_SCALE;


/*
    Ombre portee douce : rectangle sombre a peine visible
    (22% d'opacite), decale de quelques pixels en bas a
    droite. Comme le panneau plein est dessine par-dessus
    juste apres, seul ce petit liseret depasse -- effet
    d'ombre discret plutot qu'un cadre neon.
*/

gfx_fill_rect_blend(px + 4, py + 4, pw, ph, theme_shadow(), 22);


/*
    Panneau "verre depoli" : le fond deja affiche transparait
    legerement au travers de la couleur du theme (voir
    theme_panel_opacity()), au lieu d'un aplat opaque --
    c'est le coeur de l'effet glassmorphisme.
*/

gfx_fill_rect_blend(px, py, pw, ph, theme_panel(), theme_panel_opacity());


/* Bordure fine et discrete (pas de double bordure epaisse). */

gfx_draw_rect(px, py, pw, ph, theme_border());


return;

}


/* Coins */

fb_put(x, y, '+', color);

fb_put(x + width - 1, y, '+', color);

fb_put(x, y + height - 1, '+', color);

fb_put(x + width - 1, y + height - 1, '+', color);


/* Bordures horizontales */

for (int i = 1; i < width - 1; i++)
{

fb_put(x + i, y, '-', color);

fb_put(x + i, y + height - 1, '-', color);

}


/* Bordures verticales et interieur vide */

for (int j = 1; j < height - 1; j++)
{

fb_put(x, y + j, '|', color);

fb_put(x + width - 1, y + j, '|', color);


for (int i = 1; i < width - 1; i++)
{

fb_put(x + i, y + j, ' ', color);

}

}

}



void gui_draw_window(
int x, int y, int width, int height, const char* title, u8 color,
u32* bx, u32* by, u32* bsize,
u32* max_bx, u32* max_by, u32* max_bsize, int is_maximized,
u32* min_bx, u32* min_by, u32* min_bsize
)
{

gui_draw_box(x, y, width, height, color);


if (gfx_available())
{


u32 px = (u32)x * 8 * GFX_SCALE;

u32 py = (u32)y * 8 * GFX_SCALE;

u32 pw = (u32)width * 8 * GFX_SCALE;

u32 titlebar_h = 8 * GFX_SCALE + 4;


/*
    Barre de titre : quasiment opaque (95%), un peu plus
    marquee que le corps "verre depoli" de la fenetre en
    dessous -- meme logique que les barres de titre
    macOS/GNOME/Windows, qui se distinguent legerement du
    reste de la fenetre sans rompre l'effet de transparence.
*/

gfx_fill_rect_blend(px, py, pw, titlebar_h, theme_titlebar_bg(), 95);

gfx_draw_hline(px, py + titlebar_h - 1, pw, theme_border());


if (title != (void*)0)
{

/*
    Correctif (barre de titre macOS) : titre CENTRE plutot
    qu'aligne a gauche -- meme convention que macOS (les
    feux tricolores occupent desormais la gauche, voir plus
    bas, donc un titre aligne a gauche les chevaucherait).
    Centrage approximatif par nombre de caracteres (pas de
    largeur de police variable dans ce noyau -- chaque
    caractere fait exactement 8*GFX_SCALE pixels de large,
    voir gfx_draw_text()), suffisant pour une police a chasse
    fixe.
*/

u32 title_len = 0;

while (title[title_len] != 0) { title_len++; }

u32 title_px_w = title_len * 8 * GFX_SCALE;

u32 title_x = (pw > title_px_w) ? px + (pw - title_px_w) / 2 : px + 8 * GFX_SCALE;

gfx_draw_text(title_x, py + 2, title, theme_titlebar_text(), GFX_SCALE);

}


/*
    Feux tricolores macOS, alignes a GAUCHE (convention macOS --
    Windows/GNOME les alignent a droite, voir le commentaire
    precedent avant ce correctif), dans l'ordre fermer / reduire
    / agrandir. De simples disques colores (voir gfx_fill_circle(),
    kernel/drivers/graphics/graphics.c) sans glyphe dessus : le
    vrai macOS n'affiche ses glyphes (x / - / +) qu'au survol de
    la souris, un etat que ce noyau ne suit pas au pixel pres --
    trois points de couleur nette restent parfaitement
    reconnaissables sans eux.
*/

u32 dot_radius = (u32)(2 * GFX_SCALE) + 1;

u32 dot_spacing = dot_radius * 2 + (u32)(3 * GFX_SCALE);

u32 dot_cy = py + titlebar_h / 2;


u32 close_cx = px + 8 * GFX_SCALE + dot_radius;

u32 close_x = close_cx - dot_radius;

u32 button_y = dot_cy - dot_radius;

u32 button_size = dot_radius * 2;


gfx_fill_circle(close_cx, dot_cy, dot_radius, theme_close_bg());


if (bx != (void*)0) { *bx = close_x; }

if (by != (void*)0) { *by = button_y; }

if (bsize != (void*)0) { *bsize = button_size; }


u32 next_cx = close_cx;


if (min_bx != (void*)0 && min_by != (void*)0 && min_bsize != (void*)0)
{


u32 min_cx = next_cx + dot_spacing;

gfx_fill_circle(min_cx, dot_cy, dot_radius, theme_minimize_bg());


*min_bx = min_cx - dot_radius;

*min_by = dot_cy - dot_radius;

*min_bsize = button_size;


next_cx = min_cx;


}


if (max_bx != (void*)0 && max_by != (void*)0 && max_bsize != (void*)0)
{


u32 max_cx = next_cx + dot_spacing;

gfx_fill_circle(max_cx, dot_cy, dot_radius, theme_maximize_bg());


*max_bx = max_cx - dot_radius;

*max_by = dot_cy - dot_radius;

*max_bsize = button_size;


}


return;

}


/*
    Barre de titre : couleur "inversee" (on echange les
    nibbles fond/texte de l'octet couleur VGA) pour la
    distinguer visuellement du reste de la fenetre.
*/

u8 title_color = (u8)((color << 4) | (color >> 4));


for (int i = 1; i < width - 1; i++)
{

fb_put(x + i, y, ' ', title_color);

}


if (title != (void*)0)
{

int max_len = width - 3;

int i = 0;

while (title[i] != 0 && i < max_len)
{

fb_put(x + 1 + i, y, title[i], title_color);

i++;

}

}

}



#define GUI_CURSOR_SIZE 10


static u8 gui_cursor_save_buffer[GUI_CURSOR_SIZE * GUI_CURSOR_SIZE * 4];

static s32 gui_cursor_saved_x = -1;

static s32 gui_cursor_saved_y = -1;

static int gui_cursor_is_saved = 0;


/*
    Restaure le fond precedemment sauvegarde sous le curseur (
    s'il y en a un), sans rien dessiner de nouveau. A appeler
    juste avant de quitter une boucle interactive, pour ne pas
    laisser une "trace" du curseur visible.
*/

void gui_cursor_erase()
{

if (!gui_cursor_is_saved)
{

return;

}


gfx_write_rect((u32)gui_cursor_saved_x, (u32)gui_cursor_saved_y, GUI_CURSOR_SIZE, GUI_CURSOR_SIZE, gui_cursor_save_buffer);

gui_cursor_is_saved = 0;

}


void gui_cursor_reset()
{

gui_cursor_is_saved = 0;

}


void gui_draw_cursor(s32 x, s32 y)
{

if (!gfx_available())
{

return;

}

if (x < 0 || y < 0)
{

return;

}


/*
    Correctif (scintillement) : la version precedente
    redessinait toute la fenetre a chaque image rien que pour
    deplacer le curseur, ce qui donnait un effet de
    "clignotement"/"tremblement" visible a chaque
    mouvement de souris. On se contente desormais de
    restaurer le fond sous l'ancienne position du curseur
    (sauvegarde a l'appel precedent), puis de sauvegarder et
    dessiner sur la nouvelle position -- seule une petite zone
    10x10 change reellement a chaque image.
*/

gui_cursor_erase();


gfx_read_rect((u32)x, (u32)y, GUI_CURSOR_SIZE, GUI_CURSOR_SIZE, gui_cursor_save_buffer);

gui_cursor_saved_x = x;

gui_cursor_saved_y = y;

gui_cursor_is_saved = 1;


/*
    Curseur a deux tons (remplissage clair + fin contour
    sombre), plutot qu'une seule couleur pleine : un curseur
    theme_cursor() plein (blanc) devenait quasi invisible sur
    un fond clair une fois le theme clair applique. Le contour
    sombre garantit un minimum de contraste quel que soit ce
    qu'il y a en dessous (fenetre claire, sombre, ou bureau),
    comme les curseurs systeme habituels. Reste strictement
    dans la boite GUI_CURSOR_SIZE x GUI_CURSOR_SIZE deja
    sauvegardee/restauree ci-dessus par gui_cursor_erase().
*/

gfx_color fill = theme_cursor();

gfx_color outline = theme_cursor_outline();

for (u32 row = 0; row < GUI_CURSOR_SIZE; row++)
{

u32 w = GUI_CURSOR_SIZE - row;

if (w <= 2)
{

gfx_fill_rect((u32)x, (u32)y + row, w, 1, outline);

continue;

}

gfx_fill_rect((u32)x, (u32)y + row, 1, 1, outline);

gfx_fill_rect((u32)x + 1, (u32)y + row, w - 2, 1, fill);

gfx_fill_rect((u32)x + w - 1, (u32)y + row, 1, 1, outline);

}

}



int gui_point_in_button(u32 bx, u32 by, u32 bsize, s32 px, s32 py)
{

if (px < 0 || py < 0)
{

return 0;

}

u32 x = (u32)px;

u32 y = (u32)py;

return (x >= bx && x < bx + bsize && y >= by && y < by + bsize);

}



int gui_point_in_rect(u32 rx, u32 ry, u32 rw, u32 rh, s32 px, s32 py)
{

if (px < 0 || py < 0)
{

return 0;

}

u32 x = (u32)px;

u32 y = (u32)py;

return (x >= rx && x < rx + rw && y >= ry && y < ry + rh);

}



void gui_draw_button(int x, int y, int w, int h, const char* label, u32* out_x, u32* out_y, u32* out_w, u32* out_h)
{

if (!gfx_available())
{

return;

}


u32 px = (u32)x * 8 * GFX_SCALE;

u32 py = (u32)y * 8 * GFX_SCALE;

u32 pw = (u32)w * 8 * GFX_SCALE;

u32 ph = (u32)h * 8 * GFX_SCALE;


gfx_fill_rect(px, py, pw, ph, theme_button_bg());

gfx_draw_rect(px, py, pw, ph, theme_button_border());


if (label != (void*)0)
{

/*
    Centrage approximatif : decale d'environ un demi-
    caractere par colonne/ligne de marge, suffisant pour
    des libelles courts (1-3 caracteres) sur un bouton.
*/

int label_len = 0;

while (label[label_len] != 0)
{

label_len++;

}


u32 text_w = (u32)label_len * 8 * GFX_SCALE;

u32 offset_x = (pw > text_w) ? (pw - text_w) / 2 : 0;

u32 offset_y = (ph > 8 * GFX_SCALE) ? (ph - 8 * GFX_SCALE) / 2 : 0;


gfx_draw_text(px + offset_x, py + offset_y, label, theme_text(), GFX_SCALE);

}


if (out_x != (void*)0) { *out_x = px; }

if (out_y != (void*)0) { *out_y = py; }

if (out_w != (void*)0) { *out_w = pw; }

if (out_h != (void*)0) { *out_h = ph; }

}


void gui_draw_field(int x, int y, int w, int h, const char* text, int masked, int focused, u32* out_x, u32* out_y, u32* out_w, u32* out_h)
{

if (!gfx_available())
{

if (out_x != (void*)0) { *out_x = 0; }

if (out_y != (void*)0) { *out_y = 0; }

if (out_w != (void*)0) { *out_w = 0; }

if (out_h != (void*)0) { *out_h = 0; }

return;

}


u32 px = (u32)x * 8 * GFX_SCALE;

u32 py = (u32)y * 8 * GFX_SCALE;

u32 pw = (u32)w * 8 * GFX_SCALE;

u32 ph = (u32)h * 8 * GFX_SCALE;


gfx_fill_rect_blend(px, py, pw, ph, theme_button_bg(), 92);


/*
    Bordure doublee (deux rectangles imbriques) quand le champ
    a le focus, plutot qu'une simple couleur differente : reste
    clairement visible meme sur les tres petits ecrans/themes ou
    la difference de couleur seule serait trop subtile.
*/

gfx_draw_rect(px, py, pw, ph, focused ? theme_text() : theme_button_border());

if (focused && pw > 2 && ph > 2)
{

gfx_draw_rect(px + 1, py + 1, pw - 2, ph - 2, theme_text());

}


if (text != (void*)0)
{

char display[48];

int i = 0;

while (text[i] != 0 && i < 46)
{

display[i] = masked ? '*' : text[i];

i++;

}

display[i] = 0;


u32 offset_y = (ph > 8 * GFX_SCALE) ? (ph - 8 * GFX_SCALE) / 2 : 0;

gfx_draw_text(px + 6, py + offset_y, display, theme_text(), GFX_SCALE);

}


if (out_x != (void*)0) { *out_x = px; }

if (out_y != (void*)0) { *out_y = py; }

if (out_w != (void*)0) { *out_w = pw; }

if (out_h != (void*)0) { *out_h = ph; }

}


int gui_drag_update(
gui_drag* drag,
int* win_x, int* win_y,
int width, int height,
s32 mouse_x, s32 mouse_y, int mouse_down,
u32 close_bx, u32 close_by, u32 close_bsize
)
{


if (!gfx_available())
{

return 0;

}


u32 px = (u32)(*win_x) * 8 * GFX_SCALE;

u32 py = (u32)(*win_y) * 8 * GFX_SCALE;

u32 pw = (u32)width * 8 * GFX_SCALE;

u32 titlebar_h = 8 * GFX_SCALE + 4;


if (!drag->active)
{


if (!mouse_down)
{

return 0;

}


/*
    Demarre un glisser uniquement si l'appui initial tombe
    dans la barre de titre (pas n'importe ou dans la
    fenetre -- cliquer le contenu ne doit pas la deplacer,
    comme sur n'importe quel bureau habituel) ET en dehors
    du bouton de fermeture (sinon fermer la fenetre
    deplacerait aussi la fenetre juste avant qu'elle ne se
    ferme).
*/

if (!gui_point_in_rect(px, py, pw, titlebar_h, mouse_x, mouse_y))
{

return 0;

}


if (gui_point_in_rect(close_bx, close_by, close_bsize, close_bsize, mouse_x, mouse_y))
{

return 0;

}


drag->active = 1;

drag->grab_offset_x = mouse_x - (s32)px;

drag->grab_offset_y = mouse_y - (s32)py;

return 0;

}


if (!mouse_down)
{

drag->active = 0;

return 0;

}


s32 new_px = mouse_x - drag->grab_offset_x;

s32 new_py = mouse_y - drag->grab_offset_y;


int new_win_x = new_px / (8 * GFX_SCALE);

int new_win_y = new_py / (8 * GFX_SCALE);


/*
    Bornage a l'ecran : une fenetre glissee jusqu'au bord ne
    doit ni disparaitre hors champ, ni pouvoir etre "perdue"
    derriere un bord sans bouton de fermeture accessible. Un
    minimum de 2 cellules de la barre de titre reste toujours
    visible en haut/a gauche, et la fenetre entiere reste sur
    l'ecran a droite/en bas.

    Correctif CRITIQUE (souris "qui ne fonctionne plus" par
    endroits) : le bas de l'ecran n'etait borne qu'a "rows"
    (le bas ABSOLU de l'ecran), sans jamais tenir compte des 2
    dernieres lignes reservees a la barre des taches (voir
    gui/desktop.c, desktop_draw_taskbar()). Une fenetre glissee
    tout en bas pouvait donc chevaucher la barre des taches --
    et tout clic sur ses PROPRES boutons dans cette zone
    partagee etait alors intercepte par la detection "clic sur
    la barre des taches" (voir gui_window_yield_click() dans
    chaque application), qui rend la main au bureau au lieu de
    declencher le bouton reellement vise. Resultat observe :
    des boutons "qui ne repondent plus" des qu'une fenetre est
    deplacee pres du bas de l'ecran. Le meme bornage que le
    bouton Agrandir (qui, lui, respectait deja cette limite)
    est applique ici.
*/

int cols = (int)(gfx_width() / (8 * GFX_SCALE));

int rows = (int)(gfx_height() / (8 * GFX_SCALE));

int taskbar_reserved_rows = 2;


if (new_win_x < 0) { new_win_x = 0; }

if (new_win_y < 0) { new_win_y = 0; }

if (new_win_x + width > cols) { new_win_x = cols - width; if (new_win_x < 0) { new_win_x = 0; } }

if (new_win_y + height > rows - taskbar_reserved_rows) { new_win_y = rows - taskbar_reserved_rows - height; if (new_win_y < 0) { new_win_y = 0; } }


if (new_win_x == *win_x && new_win_y == *win_y)
{

return 0;

}


*win_x = new_win_x;

*win_y = new_win_y;

return 1;

}


int gui_text_prompt(const char* title, const char* label, const char* initial_value, char* buffer, int max_len)
{


if (!gfx_available())
{

return 0;

}


int i = 0;

if (initial_value != (void*)0)
{

while (initial_value[i] != 0 && i < max_len - 1)
{

buffer[i] = initial_value[i];

i++;

}

}

buffer[i] = 0;


u32 cols = gfx_width() / (8 * GFX_SCALE);

u32 rows = gfx_height() / (8 * GFX_SCALE);


int panel_w = 30;

if (panel_w > (int)cols - 4) { panel_w = (int)cols - 4; }

int panel_h = 9;

int panel_x = ((int)cols - panel_w) / 2;

int panel_y = ((int)rows - panel_h) / 2;


gui_drag drag;

drag.active = 0;


u32 field_x, field_y, field_w, field_h;

u32 ok_x, ok_y, ok_w, ok_h;

u32 cancel_x, cancel_y, cancel_w, cancel_h;

u32 close_bx, close_by, close_bsize;


int redraw = 1;

int was_pressed = 0;

int result = -1;


keyboard_flush();


while (result < 0)
{


if (redraw)
{


gui_draw_window(panel_x, panel_y, panel_w, panel_h, title, theme_text_attr(), &close_bx, &close_by, &close_bsize, (void*)0, (void*)0, (void*)0, 0, (void*)0, (void*)0, (void*)0);

gui_draw_text(panel_x + 1, panel_y + 2, label, theme_text_attr());

gui_draw_field(panel_x + 1, panel_y + 3, panel_w - 2, 2, buffer, 0, 1, &field_x, &field_y, &field_w, &field_h);

gui_draw_button(panel_x + 1, panel_y + 6, (panel_w - 3) / 2, 2, "OK", &ok_x, &ok_y, &ok_w, &ok_h);

gui_draw_button(panel_x + 2 + (panel_w - 3) / 2, panel_y + 6, (panel_w - 3) / 2, 2, "Annuler", &cancel_x, &cancel_y, &cancel_w, &cancel_h);


redraw = 0;

}


s32 mx = mouse_get_x();

s32 my = mouse_get_y();

gui_draw_cursor(mx, my);


int now_pressed = mouse_left_pressed();

int clicked = (now_pressed && !was_pressed);

was_pressed = now_pressed;


if (gui_drag_update(&drag, &panel_x, &panel_y, panel_w, panel_h, mx, my, now_pressed, close_bx, close_by, close_bsize))
{

redraw = 1;

}


if (clicked && !drag.active)
{

if (gui_point_in_rect(close_bx, close_by, close_bsize, close_bsize, mx, my))
{

result = 0;

}
else if (gui_point_in_rect(ok_x, ok_y, ok_w, ok_h, mx, my))
{

result = (buffer[0] != 0) ? 1 : 0;

}
else if (gui_point_in_rect(cancel_x, cancel_y, cancel_w, cancel_h, mx, my))
{

result = 0;

}

}


if (keyboard_available())
{

char c = keyboard_getchar();


if (c == '\n')
{

result = (buffer[0] != 0) ? 1 : 0;

}
else if (c == '\b')
{

u32 len = mk_strlen(buffer);

if (len > 0) { buffer[len - 1] = 0; }


/*
    Correctif (ecran qui scintille a chaque frappe) : ne
    redessiner QUE le champ (gui_draw_field() redessine deja
    entierement son propre fond), pas toute la fenetre
    (cadre + les deux boutons) comme le faisait
    auparavant "redraw = 1".
*/

gui_draw_field(panel_x + 1, panel_y + 3, panel_w - 2, 2, buffer, 0, 1, &field_x, &field_y, &field_w, &field_h);

}
else if (c != 0)
{

u32 len = mk_strlen(buffer);

if ((int)len < max_len - 1) { buffer[len] = c; buffer[len + 1] = 0; }


gui_draw_field(panel_x + 1, panel_y + 3, panel_w - 2, 2, buffer, 0, 1, &field_x, &field_y, &field_w, &field_h);

}

}


unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{
}


}


gui_cursor_erase();


return result;


}
