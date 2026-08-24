#include "settings.h"

#include "../session/session.h"
#include "../../gui/gui.h"

#include "../../gui/theme.h"
#include "../../gui/desktop.h"
#include "../../gui/window.h"
#include "../../gui/assets/wallpaper_images.h"
#include "../../kernel/drivers/graphics/graphics.h"
#include "../../kernel/drivers/mouse/mouse.h"

#include "../../kernel/drivers/speaker/speaker.h"

#include "../../filesystem/block.h"
#include "../../shell/msh.h"


void console_write(const char* text);
void console_clear();
void keyboard_flush();
int keyboard_available();
char keyboard_getchar();
unsigned long timer_ticks();


#define GFX_SCALE 2


void cmd_settings()
{


if (!gfx_available())
{

console_write("Les parametres graphiques necessitent le mode graphique et une souris.\n");

return;

}


int slot = gui_window_claim_slot();

if (slot < 0)
{

return;

}


int win_x = 2, win_y = 2, win_w = 34, win_h = 27;


/*
    Filet de securite : borne la hauteur pour qu'elle ne
    depasse jamais l'espace disponible au-dessus de la barre
    des taches, meme a la plus petite resolution prise en
    charge (640x480, voir boot/loader/stage2.asm) -- une
    fenetre qui deborderait dans la bande reservee a la barre
    des taches verrait ses propres boutons du bas pris a tort
    pour des clics sur cette barre (voir le commentaire sur
    gui_window_yield_click() plus bas dans cette boucle).
*/

{

int max_win_h = (int)(gfx_height() / (8 * GFX_SCALE)) - 2 - win_y;

if (win_h > max_win_h)
{

win_h = max_win_h;

}

}


int maximized = 0;

int saved_x = win_x, saved_y = win_y, saved_w = win_w, saved_h = win_h;


u32 logout_x, logout_y, logout_w, logout_h;

u32 theme_btn_x, theme_btn_y, theme_btn_w, theme_btn_h;

u32 sound_btn_x, sound_btn_y, sound_btn_w, sound_btn_h;

u32 pattern_btn_x, pattern_btn_y, pattern_btn_w, pattern_btn_h;

u32 close_bx = 0, close_by = 0, close_bsize = 0;

u32 max_bx = 0, max_by = 0, max_bsize = 0;

u32 min_bx = 0, min_by = 0, min_bsize = 0;


/*
    6 photos embarquees (voir gui/assets/wallpaper_images.h) --
    meme nombre que wallpaper_photo_count() a l'execution;
    fige ici en dur (pas d'utilisation d'une fonction dans une
    taille de tableau, qui doit rester une constante de
    compilation).
*/

#define SETTINGS_SWATCH_COUNT 6

u32 swatch_x[SETTINGS_SWATCH_COUNT], swatch_y[SETTINGS_SWATCH_COUNT];

u32 swatch_w[SETTINGS_SWATCH_COUNT], swatch_h[SETTINGS_SWATCH_COUNT];


int was_pressed = 0;

int redraw = 1;

gui_drag drag;

drag.active = 0;


while (1)
{


if (!gui_window_has_focus(slot))
{

redraw = 1;

gui_window_idle();

continue;

}


/*
    Le theme (clair/sombre) ou le fond d'ecran peuvent
    changer pendant que cette fenetre est ouverte -- voir
    plus bas. "redraw" force alors un nouveau passage complet
    (desktop_render_backdrop() efface tout l'ecran avec le
    nouveau theme/fond et redessine la barre des taches).
    Sert aussi apres un glisser-deposer, un agrandissement/
    restauration, ou une reprise de focus (voir plus haut).
*/

if (redraw)
{


desktop_render_backdrop();

gui_draw_window(
win_x, win_y, win_w, win_h, "Parametres", theme_text_attr(),
&close_bx, &close_by, &close_bsize,
&max_bx, &max_by, &max_bsize, maximized,
&min_bx, &min_by, &min_bsize
);

gui_draw_session_content(win_x, win_y);

gui_draw_button(win_x + 1, win_y + win_h - 6, win_w - 2, 2, "Deconnexion", &logout_x, &logout_y, &logout_w, &logout_h);

gui_draw_button(
win_x + 1, win_y + win_h - 4, win_w - 2, 2,
theme_is_dark() ? "Theme : Sombre" : "Theme : Clair",
&theme_btn_x, &theme_btn_y, &theme_btn_w, &theme_btn_h
);


/*
    Selecteur de fond d'ecran : d'abord les VRAIES photos
    (voir gui/assets/wallpaper_images.h), en vignettes
    cliquables avec un aperçu miniature reel -- celle
    actuellement active a une bordure doublee (meme
    convention que gui_draw_field() pour un champ ayant le
    focus). Les motifs procéduraux d'origine (gui/theme.h,
    wallpaper_style) restent disponibles via un simple bouton
    qui fait defiler les 6 -- pas une deuxieme grille de
    vignettes, pour ne pas doubler la hauteur de cette
    fenetre.
*/

gui_draw_text(win_x + 1, win_y + 11, "Fond d'ecran :", theme_text_attr());


int swatch_cell_w = 4, swatch_cell_h = 3;

int current_photo = wallpaper_get_photo_index();


for (int i = 0; i < SETTINGS_SWATCH_COUNT; i++)
{

int sx = win_x + 1 + i * (swatch_cell_w + 1);

int sy = win_y + 12;


u32 px = (u32)sx * 8 * GFX_SCALE;

u32 py = (u32)sy * 8 * GFX_SCALE;

u32 pw = (u32)swatch_cell_w * 8 * GFX_SCALE;

u32 ph = (u32)swatch_cell_h * 8 * GFX_SCALE;


gui_paint_photo_area(px, py, pw, ph, (u32)i);


gfx_draw_rect(px, py, pw, ph, (current_photo == i) ? theme_text() : theme_border());

if (current_photo == i && pw > 2 && ph > 2)
{

gfx_draw_rect(px + 1, py + 1, pw - 2, ph - 2, theme_text());

}


swatch_x[i] = px;

swatch_y[i] = py;

swatch_w[i] = pw;

swatch_h[i] = ph;

}


{

char pattern_label[40];

const char* prefix = "Motif procedural : ";

int pi = 0;

while (prefix[pi] && pi < 39) { pattern_label[pi] = prefix[pi]; pi++; }

const char* style_name = wallpaper_style_name(wallpaper_get_style());

int si = 0;

while (style_name[si] && pi < 39) { pattern_label[pi++] = style_name[si++]; }

pattern_label[pi] = 0;


gui_draw_button(win_x + 1, win_y + 15, win_w - 2, 2, pattern_label, &pattern_btn_x, &pattern_btn_y, &pattern_btn_w, &pattern_btn_h);

}


/*
    Reglage sonore (voir kernel/drivers/speaker) : coupe-
    circuit global -- un clic joue immediatement un son de
    test (sauf en train de le desactiver, evidemment), pour
    verifier le reglage sans avoir a declencher un evenement
    reel (erreur, deconnexion...).
*/

gui_draw_button(
win_x + 1, win_y + 18, win_w - 2, 2,
sound_is_enabled() ? "Son : Active (cliquer pour tester)" : "Son : Desactive",
&sound_btn_x, &sound_btn_y, &sound_btn_w, &sound_btn_h
);


/*
    Espace disque : information systeme de base, absente
    jusqu'ici de tout le systeme (aucune commande shell ni
    aucun ecran ne l'affichait). Calculee a chaque redessin
    (pas a chaque image) en comptant les blocs libres parmi
    TOTAL_BLOCKS (filesystem/block.h) -- 2048 iterations tout
    au plus, negligeable pour un evenement aussi rare qu'un
    redessin de cette fenetre.
*/

{

u32 free_blocks = 0;

for (u32 b = 0; b < TOTAL_BLOCKS; b++)
{

if (block_is_free(b))
{

free_blocks++;

}

}


u32 free_kb = (free_blocks * BLOCK_SIZE) / 1024;

u32 total_kb = (TOTAL_BLOCKS * BLOCK_SIZE) / 1024;


char line[48];

const char* label = "Disque : ";

int i = 0;

while (label[i] && i < 47) { line[i] = label[i]; i++; }


u32 v = free_kb;

char tmp[12];

int t = 0;

if (v == 0) { tmp[t++] = '0'; }

while (v > 0) { tmp[t++] = (char)('0' + (v % 10)); v /= 10; }

while (t > 0 && i < 47) { t--; line[i++] = tmp[t]; }


const char* mid = " Ko libres sur ";

int m = 0;

while (mid[m] && i < 47) { line[i++] = mid[m]; m++; }


v = total_kb;

t = 0;

if (v == 0) { tmp[t++] = '0'; }

while (v > 0) { tmp[t++] = (char)('0' + (v % 10)); v /= 10; }

while (t > 0 && i < 47) { t--; line[i++] = tmp[t]; }


const char* suffix = " Ko";

int s = 0;

while (suffix[s] && i < 47) { line[i++] = suffix[s]; s++; }


line[i] = 0;


gui_draw_text(win_x + 1, win_y + 21, line, theme_text_attr());

}


was_pressed = mouse_left_pressed();

redraw = 0;

}


s32 mx = mouse_get_x();

s32 my = mouse_get_y();

gui_draw_cursor(mx, my);


int now_pressed = mouse_left_pressed();

int clicked = (now_pressed && !was_pressed);

was_pressed = now_pressed;


/*
    Correctif (barre des taches non cliquable pendant que
    cette fenetre a le focus) : voir le meme commentaire dans
    apps/calculator/calculator.c.
*/

if (clicked)
{

u32 rows_now = gfx_height() / (8 * GFX_SCALE);

u32 taskbar_top_px = (rows_now - 2) * 8 * GFX_SCALE;

if ((u32)my >= taskbar_top_px)
{

gui_cursor_erase();

gui_window_yield_click(slot);

continue;

}

}


if (!maximized && gui_drag_update(&drag, &win_x, &win_y, win_w, win_h, mx, my, now_pressed, close_bx, close_by, close_bsize))
{

redraw = 1;

}


if (clicked && gui_point_in_rect(close_bx, close_by, close_bsize, close_bsize, mx, my))
{

gui_cursor_erase();

gui_window_close(slot);

break;

}


else if (clicked && gui_point_in_rect(min_bx, min_by, min_bsize, min_bsize, mx, my))
{

gui_cursor_erase();

gui_window_minimize(slot);

continue;

}


else if (clicked && gui_point_in_rect(max_bx, max_by, max_bsize, max_bsize, mx, my))
{

if (!maximized)
{

saved_x = win_x; saved_y = win_y; saved_w = win_w; saved_h = win_h;

win_x = 0;

win_y = 0;

win_w = (int)(gfx_width() / (8 * GFX_SCALE));

win_h = (int)(gfx_height() / (8 * GFX_SCALE)) - 2;

maximized = 1;

}
else
{

win_x = saved_x; win_y = saved_y; win_w = saved_w; win_h = saved_h;

maximized = 0;

}

redraw = 1;

was_pressed = 0;

}


else if (clicked && !drag.active && gui_point_in_rect(logout_x, logout_y, logout_w, logout_h, mx, my))
{

/*
    shell_request_logout() (shell/msh.c) pose un drapeau lu
    par gui_desktop_run() (gui/desktop.c) a chaque tour de
    boucle -- pas par ce thread-ci (chaque fenetre tourne
    desormais dans son propre thread, voir gui/window.h) :
    on se contente donc de poser le drapeau et de fermer
    cette fenetre normalement, le bureau se chargera de la
    deconnexion reelle des qu'il regagnera le focus.
*/

shell_request_logout();

gui_cursor_erase();

gui_window_close(slot);

break;

}


else if (clicked && !drag.active && gui_point_in_rect(theme_btn_x, theme_btn_y, theme_btn_w, theme_btn_h, mx, my))
{

theme_toggle_dark_mode();

redraw = 1;

}


else if (clicked && !drag.active && gui_point_in_rect(sound_btn_x, sound_btn_y, sound_btn_w, sound_btn_h, mx, my))
{

if (sound_is_enabled())
{

/* Joue le son de test AVANT de couper -- desactiver puis tester serait toujours silencieux. */

sound_play_startup();

sound_set_enabled(0);

}
else
{

sound_set_enabled(1);

sound_play_startup();

}

redraw = 1;

}


else if (clicked && !drag.active && gui_point_in_rect(pattern_btn_x, pattern_btn_y, pattern_btn_w, pattern_btn_h, mx, my))
{

/*
    Fait defiler les 6 motifs procéduraux (voir gui/theme.h)
    -- wallpaper_set_style() choisit aussi automatiquement le
    mode "motif" (desactive la photo selectionnee, voir
    theme.c).
*/

wallpaper_style next = (wallpaper_style)((wallpaper_get_style() + 1) % WALLPAPER_STYLE_COUNT);

wallpaper_set_style(next);

redraw = 1;

}


else if (clicked && !drag.active)
{

for (int i = 0; i < SETTINGS_SWATCH_COUNT; i++)
{

if (gui_point_in_rect(swatch_x[i], swatch_y[i], swatch_w[i], swatch_h[i], mx, my))
{

wallpaper_set_photo_index(i);

redraw = 1;

break;

}

}

}


if (keyboard_available())
{

char c = keyboard_getchar();

if (c == '\n')
{

gui_cursor_erase();

gui_window_close(slot);

break;

}

}


unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{
}


}


}
