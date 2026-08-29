#include "file_manager.h"

#include "../../gui/gui.h"

#include "../../gui/theme.h"
#include "../../gui/icons.h"
#include "../../gui/desktop.h"
#include "../../gui/window.h"
#include "../../kernel/drivers/graphics/graphics.h"
#include "../../kernel/drivers/mouse/mouse.h"
#include "../../filesystem/inode.h"
#include "../../filesystem/file.h"
#include "../../filesystem/directory.h"
#include "../../libc/string.h"


void console_write(const char* text);
void console_clear();
void keyboard_flush();
int keyboard_available();
char keyboard_getchar();
unsigned long timer_ticks();


#define FM_MAX_ROWS 10

#define FM_VISIBLE_ROWS 8

#define GFX_SCALE 2


/*
    Modes de la fenetre. La Corbeille (auparavant un troisieme
    et quatrieme mode ici) est desormais sa propre application
    (voir apps/trash/trash.c) -- accessible depuis le Centre
    d'applications, comme demande : "la corbeille est un autre
    programme/dossier", pas un onglet cache dans l'explorateur.
*/

#define FM_MODE_LIST 0

#define FM_MODE_VIEW 1


typedef struct
{

u32 x, y, w, h;

} fm_hitbox;


/*
    Dossiers standards (Documents, Images, etc.), crees a la
    racine des le premier passage dans l'explorateur -- meme
    principe que le dossier personnel pre-rempli de Windows/
    macOS/GNOME a la premiere connexion. directory_create()
    (filesystem/directory.c) renvoie simplement 0 sans rien
    faire si le nom existe deja (fichier ou dossier) : appeler
    cette fonction a chaque ouverture de l'explorateur est donc
    sans danger (idempotent), pas seulement au tout premier
    demarrage.
*/

static void fm_ensure_standard_folders()
{

directory_create("Documents", "");

directory_create("Images", "");

directory_create("Videos", "");

directory_create("Musique", "");

directory_create("Telechargements", "");

directory_create("Bureau", "");

}


void cmd_files()
{


if (!gfx_available())
{

console_write("L'explorateur de fichiers necessite le mode graphique et une souris.\n");

return;

}


int slot = gui_window_claim_slot();

if (slot < 0)
{

return;

}


fm_ensure_standard_folders();


int win_x = 2, win_y = 2, win_w = 34, win_h = 20;


int maximized = 0;

int saved_x = win_x, saved_y = win_y, saved_w = win_w, saved_h = win_h;


char current_parent[FILE_NAME_SIZE];

current_parent[0] = 0;


char names[FM_MAX_ROWS][FILE_NAME_SIZE];

int is_dir[FM_MAX_ROWS];

u32 name_count = directory_list_children(current_parent, names, is_dir, FM_MAX_ROWS);


int mode = FM_MODE_LIST;

int viewing = -1;


fm_hitbox row[FM_MAX_ROWS];

fm_hitbox up_link;

int has_up_link = 0;

fm_hitbox new_file_btn, new_folder_btn;

fm_hitbox delete_btn, rename_btn;

u32 close_bx = 0, close_by = 0, close_bsize = 0;

u32 max_bx = 0, max_by = 0, max_bsize = 0;

u32 min_bx = 0, min_by = 0, min_bsize = 0;


int redraw = 1;

int was_pressed = 0;

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


if (redraw)
{


desktop_render_backdrop();


if (mode == FM_MODE_LIST)
{


gui_draw_window(
win_x, win_y, win_w, win_h, "Explorateur de fichiers", theme_text_attr(),
&close_bx, &close_by, &close_bsize,
&max_bx, &max_by, &max_bsize, maximized,
&min_bx, &min_by, &min_bsize
);


int row_y = win_y + 2;


has_up_link = (current_parent[0] != 0);

if (has_up_link)
{

gui_draw_button(win_x + 1, row_y, win_w - 2, 1, "<< Retour", &up_link.x, &up_link.y, &up_link.w, &up_link.h);

row_y++;

}


if (name_count == 0)
{

gui_draw_text(win_x + 1, row_y, "(dossier vide)", theme_text_attr());

}


u32 visible = (name_count < FM_VISIBLE_ROWS) ? name_count : FM_VISIBLE_ROWS;

for (u32 i = 0; i < visible; i++)
{


if (is_dir[i])
{

icon_draw_folder((u32)(win_x + 1) * 8 * GFX_SCALE, (u32)(row_y + (int)i) * 8 * GFX_SCALE, 8 * GFX_SCALE, theme_text());

gui_draw_button(win_x + 3, row_y + (int)i, win_w - 4, 1, names[i], &row[i].x, &row[i].y, &row[i].w, &row[i].h);

}
else
{

gui_draw_button(win_x + 1, row_y + (int)i, win_w - 2, 1, names[i], &row[i].x, &row[i].y, &row[i].w, &row[i].h);

}

}


gui_draw_button(win_x + 1, win_y + win_h - 5, (win_w - 3) / 2, 2, "Nouveau fichier", &new_file_btn.x, &new_file_btn.y, &new_file_btn.w, &new_file_btn.h);

gui_draw_button(win_x + 2 + (win_w - 3) / 2, win_y + win_h - 5, (win_w - 3) / 2, 2, "Nouveau dossier", &new_folder_btn.x, &new_folder_btn.y, &new_folder_btn.w, &new_folder_btn.h);

}

else /* FM_MODE_VIEW */
{


gui_draw_window(
win_x, win_y, win_w, win_h, names[viewing], theme_text_attr(),
&close_bx, &close_by, &close_bsize,
&max_bx, &max_by, &max_bsize, maximized,
&min_bx, &min_by, &min_bsize
);


char* file_content = file_read(names[viewing]);

if (file_content != (void*)0)
{

gui_draw_text(win_x + 1, win_y + 2, file_content, theme_text_attr());

}
else
{

gui_draw_text(win_x + 1, win_y + 2, "(impossible de lire ce fichier)", theme_text_attr());

}


gui_draw_button(win_x + 1, win_y + win_h - 6, win_w - 2, 2, "Renommer", &rename_btn.x, &rename_btn.y, &rename_btn.w, &rename_btn.h);

gui_draw_button(win_x + 1, win_y + win_h - 4, win_w - 2, 2, "Supprimer (vers la corbeille)", &delete_btn.x, &delete_btn.y, &delete_btn.w, &delete_btn.h);

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

/*
    Correctif (Dock macOS, voir gui/desktop.h) : la limite
    utilisee ici venait auparavant d'un calcul local suppose
    ("les 2 dernieres lignes de texte"), perime depuis le
    passage a un Dock flottant de hauteur fixe -- voir
    desktop_dock_top_px().
*/

if ((u32)my >= desktop_dock_top_px())
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


else if (clicked && !drag.active)
{


if (mode == FM_MODE_LIST)
{


if (has_up_link && gui_point_in_rect(up_link.x, up_link.y, up_link.w, up_link.h, mx, my))
{


/*
    Remonte au parent DU dossier courant (pas a la
    racine directement) : chaque dossier connait deja
    son propre parent (voir inode.h, champ "parent").
*/

inode* current_node = inode_find(current_parent);

if (current_node != 0)
{

mk_strcpy(current_parent, current_node->parent, FILE_NAME_SIZE);

}
else
{

current_parent[0] = 0;

}


name_count = directory_list_children(current_parent, names, is_dir, FM_MAX_ROWS);

redraw = 1;

}

else if (gui_point_in_rect(new_file_btn.x, new_file_btn.y, new_file_btn.w, new_file_btn.h, mx, my))
{

char new_name[FILE_NAME_SIZE];

new_name[0] = 0;

if (gui_text_prompt("Nouveau fichier", "Nom du fichier :", "", new_name, FILE_NAME_SIZE))
{

file_create_in(new_name, current_parent);

name_count = directory_list_children(current_parent, names, is_dir, FM_MAX_ROWS);

}

redraw = 1;

}

else if (gui_point_in_rect(new_folder_btn.x, new_folder_btn.y, new_folder_btn.w, new_folder_btn.h, mx, my))
{

char new_name[FILE_NAME_SIZE];

new_name[0] = 0;

if (gui_text_prompt("Nouveau dossier", "Nom du dossier :", "", new_name, FILE_NAME_SIZE))
{

directory_create(new_name, current_parent);

name_count = directory_list_children(current_parent, names, is_dir, FM_MAX_ROWS);

}

redraw = 1;

}

else
{

u32 visible = (name_count < FM_VISIBLE_ROWS) ? name_count : FM_VISIBLE_ROWS;

for (u32 i = 0; i < visible; i++)
{

if (gui_point_in_rect(row[i].x, row[i].y, row[i].w, row[i].h, mx, my))
{

if (is_dir[i])
{

mk_strcpy(current_parent, names[i], FILE_NAME_SIZE);

name_count = directory_list_children(current_parent, names, is_dir, FM_MAX_ROWS);

}
else
{

viewing = (int)i;

mode = FM_MODE_VIEW;

}

redraw = 1;

break;

}

}

}

}

else /* FM_MODE_VIEW */
{


if (gui_point_in_rect(delete_btn.x, delete_btn.y, delete_btn.w, delete_btn.h, mx, my))
{

file_trash(names[viewing]);

name_count = directory_list_children(current_parent, names, is_dir, FM_MAX_ROWS);

mode = FM_MODE_LIST;

redraw = 1;

}

else if (gui_point_in_rect(rename_btn.x, rename_btn.y, rename_btn.w, rename_btn.h, mx, my))
{

char new_name[FILE_NAME_SIZE];

if (gui_text_prompt("Renommer", "Nouveau nom :", names[viewing], new_name, FILE_NAME_SIZE))
{

if (file_rename(names[viewing], new_name))
{

mk_strcpy(names[viewing], new_name, FILE_NAME_SIZE);

}

}

redraw = 1;

}
else
{

mode = FM_MODE_LIST;

redraw = 1;

}

}

}


if (keyboard_available())
{

char c = keyboard_getchar();

if (c == '\n')
{

if (mode == FM_MODE_LIST)
{

gui_cursor_erase();

gui_window_close(slot);

break;

}

mode = FM_MODE_LIST;

redraw = 1;

}

}


unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{
}


}


}
