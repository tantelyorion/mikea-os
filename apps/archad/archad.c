#include "archad.h"

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
void sound_play_error();
void* mk_memcpy(void* dest, const void* src, u32 len);


#define ARCHAD_MAX_ROWS 12

#define GFX_SCALE 2


#define ARCHAD_MODE_LIST 0

#define ARCHAD_MODE_VIEW 1


/*
    Format OAR-lite -- voir archad.h pour le detail et les
    differences assumees avec le format de l'original.
*/

#define OAR_SIGNATURE_0 'O'
#define OAR_SIGNATURE_1 'A'
#define OAR_SIGNATURE_2 'R'
#define OAR_SIGNATURE_3 '1'

#define OAR_MAX_ENTRIES 16


typedef struct
{

char signature[4];

u32 file_count;

} oar_header;


typedef struct
{

char name[FILE_NAME_SIZE];

u32 size;

u32 offset;

} oar_entry;


typedef struct
{

u32 x, y, w, h;

} archad_hitbox;


/*
    Balaye la racine (voir directory_list_children(),
    filesystem/directory.h) et ne retient que les FICHIERS (pas
    les dossiers -- une archive OAR-lite regroupe du contenu de
    fichiers, pas une hierarchie de dossiers, meme limite que
    le format de l'original pour son mode OAR).
*/

static u32 archad_scan(char names[][FILE_NAME_SIZE], u32 sizes[], u32 max_count)
{

char raw_names[ARCHAD_MAX_ROWS][FILE_NAME_SIZE];

int raw_is_dir[ARCHAD_MAX_ROWS];


u32 raw_count = directory_list_children("", raw_names, raw_is_dir, ARCHAD_MAX_ROWS);


u32 count = 0;

for (u32 i = 0; i < raw_count && count < max_count; i++)
{

if (raw_is_dir[i])
{

continue;

}


int j = 0;

while (raw_names[i][j] != 0 && j < FILE_NAME_SIZE - 1) { names[count][j] = raw_names[i][j]; j++; }

names[count][j] = 0;


inode* node = inode_find(raw_names[i]);

sizes[count] = (node != 0) ? node->size : 0;


count++;

}


return count;

}


/*
    Verifie qu'un nom se termine par ".oar" (comparaison
    simplifiee, minuscules attendues comme partout ailleurs
    dans ce noyau).
*/

static int archad_has_oar_extension(const char* name)
{

u32 len = 0;

while (name[len] != 0) { len++; }


if (len < 4) { return 0; }


return name[len-4]=='.' && name[len-3]=='o' && name[len-2]=='a' && name[len-1]=='r';

}


/*
    Construit une archive OAR-lite a partir des fichiers root
    dont "selected[i]" est vrai, et l'ecrit dans "archive_name"
    via file_write_bin() (filesystem/file.h). Renvoie 1 en cas
    de succes, 0 si rien n'est selectionne ou si le total
    (en-tete + table d'entrees + donnees) depasserait
    MAX_FILE_SIZE_BIN (64 Ko, voir archad.h) -- verifie AVANT
    d'ecrire quoi que ce soit, pas de fichier partiel en cas de
    refus.
*/

static u8 g_file_buffer[65536];
static u8 g_archive_buffer[65536];

static int archad_create_archive(char names[][FILE_NAME_SIZE], u32 sizes[], int* selected, u32 count, const char* archive_name)
{

u32 selected_count = 0;

u32 total_data = 0;


for (u32 i = 0; i < count; i++)
{

if (selected[i])
{

selected_count++;

total_data += sizes[i];

}

}


if (selected_count == 0 || selected_count > OAR_MAX_ENTRIES)
{

return 0;

}


u32 header_size = (u32)sizeof(oar_header) + selected_count * (u32)sizeof(oar_entry);

u32 total_size = header_size + total_data;


if (total_size > sizeof(g_archive_buffer))
{

return 0;

}


oar_header header;

header.signature[0] = OAR_SIGNATURE_0;

header.signature[1] = OAR_SIGNATURE_1;

header.signature[2] = OAR_SIGNATURE_2;

header.signature[3] = OAR_SIGNATURE_3;

header.file_count = selected_count;


mk_memcpy(g_archive_buffer, &header, sizeof(oar_header));


u32 entry_cursor = (u32)sizeof(oar_header);

u32 data_cursor = header_size;


for (u32 i = 0; i < count; i++)
{

if (!selected[i])
{

continue;

}


oar_entry entry;

int j = 0;

while (names[i][j] != 0 && j < FILE_NAME_SIZE - 1) { entry.name[j] = names[i][j]; j++; }

entry.name[j] = 0;

entry.size = sizes[i];

entry.offset = data_cursor;


mk_memcpy(g_archive_buffer + entry_cursor, &entry, sizeof(oar_entry));

entry_cursor += (u32)sizeof(oar_entry);


u32 read_len = file_read_bin(names[i], g_file_buffer, sizeof(g_file_buffer));

mk_memcpy(g_archive_buffer + data_cursor, g_file_buffer, read_len);

data_cursor += sizes[i];

}


return file_write_bin((char*)archive_name, g_archive_buffer, total_size);

}


/*
    Charge l'archive "archive_name" en memoire et remplit
    "out_entries"/"out_count" -- utilise a la fois pour
    afficher son contenu (ARCHAD_MODE_VIEW) et pour l'extraire
    (archad_extract_all()). Renvoie 0 si le fichier n'existe
    pas ou n'a pas la signature OAR-lite attendue.
*/

static u8 g_archive_cache[65536];

static int archad_load_archive(const char* archive_name, oar_entry* out_entries, u32* out_count)
{

u32 len = file_read_bin((char*)archive_name, g_archive_cache, sizeof(g_archive_cache));

if (len < sizeof(oar_header))
{

return 0;

}


oar_header header;

mk_memcpy(&header, g_archive_cache, sizeof(oar_header));


if (header.signature[0] != OAR_SIGNATURE_0 || header.signature[1] != OAR_SIGNATURE_1 ||
header.signature[2] != OAR_SIGNATURE_2 || header.signature[3] != OAR_SIGNATURE_3)
{

return 0;

}


if (header.file_count > OAR_MAX_ENTRIES)
{

return 0;

}


mk_memcpy(out_entries, g_archive_cache + sizeof(oar_header), header.file_count * sizeof(oar_entry));

*out_count = header.file_count;


return 1;

}


/*
    Extrait TOUS les fichiers de l'archive deja chargee dans
    "g_archive_cache" (voir archad_load_archive()) a la racine,
    en ecrasant tout fichier de meme nom deja present (comme la
    plupart des extracteurs -- l'original ne demande pas non
    plus confirmation par fichier).
*/

static u32 archad_extract_all(oar_entry* entries, u32 count)
{

u32 extracted = 0;

for (u32 i = 0; i < count; i++)
{

if (file_write_bin(entries[i].name, g_archive_cache + entries[i].offset, entries[i].size))
{

extracted++;

}

}

return extracted;

}


void cmd_archad_app()
{


if (!gfx_available())
{

console_write("Archad necessite le mode graphique et une souris.\n");

return;

}


int slot = gui_window_claim_slot();

if (slot < 0)
{

return;

}


int win_x = 2, win_y = 2, win_w = 34, win_h = 20;

int maximized = 0;

int saved_x = win_x, saved_y = win_y, saved_w = win_w, saved_h = win_h;


char names[ARCHAD_MAX_ROWS][FILE_NAME_SIZE];

u32 sizes[ARCHAD_MAX_ROWS];

int selected[ARCHAD_MAX_ROWS];

u32 count = archad_scan(names, sizes, ARCHAD_MAX_ROWS);

for (u32 i = 0; i < ARCHAD_MAX_ROWS; i++) { selected[i] = 0; }


char archive_name[FILE_NAME_SIZE];

archive_name[0] = 'a'; archive_name[1] = 'r'; archive_name[2] = 'c'; archive_name[3] = 'h';

archive_name[4] = 'i'; archive_name[5] = 'v'; archive_name[6] = 'e'; archive_name[7] = '.';

archive_name[8] = 'o'; archive_name[9] = 'a'; archive_name[10] = 'r'; archive_name[11] = 0;


int mode = ARCHAD_MODE_LIST;

char status_line[40];

status_line[0] = 0;


oar_entry view_entries[OAR_MAX_ENTRIES];

u32 view_count = 0;

char viewing_name[FILE_NAME_SIZE];

viewing_name[0] = 0;


archad_hitbox row[ARCHAD_MAX_ROWS];

archad_hitbox view_row[OAR_MAX_ENTRIES];

archad_hitbox name_field_box, create_btn, extract_btn, back_btn;

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


if (mode == ARCHAD_MODE_LIST)
{


gui_draw_window(
win_x, win_y, win_w, win_h, "Archad", theme_text_attr(),
&close_bx, &close_by, &close_bsize,
&max_bx, &max_by, &max_bsize, maximized,
&min_bx, &min_by, &min_bsize
);


gui_draw_text(win_x + 1, win_y + 1, "Fichiers a la racine (case a cocher) :", theme_text_attr());


for (u32 i = 0; i < count; i++)
{

u32 py = (u32)(win_y + 2 + (int)i) * 8 * GFX_SCALE;

u32 px = (u32)(win_x + 1) * 8 * GFX_SCALE;

u32 box_size = 8 * GFX_SCALE;


gfx_draw_rect(px, py, box_size, box_size, theme_border());

if (selected[i])
{

gfx_fill_rect(px + 2, py + 2, box_size - 4, box_size - 4, theme_accent());

}


gui_draw_button(win_x + 3, win_y + 2 + (int)i, win_w - 4, 1, names[i], &row[i].x, &row[i].y, &row[i].w, &row[i].h);

}


if (count == 0)
{

gui_draw_text(win_x + 1, win_y + 2, "(aucun fichier a la racine)", theme_text_attr());

}


gui_draw_text(win_x + 1, win_y + win_h - 9, "Nom de l'archive :", theme_text_attr());

gui_draw_field(win_x + 1, win_y + win_h - 8, win_w - 2, 2, archive_name, 0, 1, &name_field_box.x, &name_field_box.y, &name_field_box.w, &name_field_box.h);


gui_draw_button(win_x + 1, win_y + win_h - 5, win_w - 2, 2, "Creer l'archive", &create_btn.x, &create_btn.y, &create_btn.w, &create_btn.h);


if (status_line[0] != 0)
{

gui_draw_text(win_x + 1, win_y + win_h - 2, status_line, theme_text_attr());

}
else
{

gui_draw_text(win_x + 1, win_y + win_h - 2, "Cliquez un .oar pour l'ouvrir/l'extraire.", theme_text_attr());

}


}
else /* ARCHAD_MODE_VIEW */
{


gui_draw_window(
win_x, win_y, win_w, win_h, viewing_name, theme_text_attr(),
&close_bx, &close_by, &close_bsize,
&max_bx, &max_by, &max_bsize, maximized,
&min_bx, &min_by, &min_bsize
);


gui_draw_text(win_x + 1, win_y + 1, "Contenu de l'archive :", theme_text_attr());


for (u32 i = 0; i < view_count; i++)
{

icon_draw_disk((u32)(win_x + 1) * 8 * GFX_SCALE, (u32)(win_y + 2 + (int)i) * 8 * GFX_SCALE, 8 * GFX_SCALE, theme_text());

gui_draw_button(win_x + 3, win_y + 2 + (int)i, win_w - 4, 1, view_entries[i].name, &view_row[i].x, &view_row[i].y, &view_row[i].w, &view_row[i].h);

}


gui_draw_button(win_x + 1, win_y + win_h - 5, win_w - 2, 2, "Extraire tout a la racine", &extract_btn.x, &extract_btn.y, &extract_btn.w, &extract_btn.h);

gui_draw_button(win_x + 1, win_y + win_h - 2, win_w - 2, 2, "Retour", &back_btn.x, &back_btn.y, &back_btn.w, &back_btn.h);


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


if (clicked)
{

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


if (mode == ARCHAD_MODE_LIST)
{


int handled = 0;


for (u32 i = 0; i < count && !handled; i++)
{

if (gui_point_in_rect(row[i].x, row[i].y, row[i].w, row[i].h, mx, my))
{

handled = 1;


if (archad_has_oar_extension(names[i]))
{

if (archad_load_archive(names[i], view_entries, &view_count))
{

int k = 0;

while (names[i][k] != 0 && k < FILE_NAME_SIZE - 1) { viewing_name[k] = names[i][k]; k++; }

viewing_name[k] = 0;

mode = ARCHAD_MODE_VIEW;

}
else
{

sound_play_error();

}

}
else
{

selected[i] = !selected[i];

}


redraw = 1;

}

}


if (!handled && gui_point_in_rect(create_btn.x, create_btn.y, create_btn.w, create_btn.h, mx, my))
{

if (archad_create_archive(names, sizes, selected, count, archive_name))
{

int s = 0;

const char* ok_msg = "Archive creee.";

while (ok_msg[s] != 0) { status_line[s] = ok_msg[s]; s++; }

status_line[s] = 0;


count = archad_scan(names, sizes, ARCHAD_MAX_ROWS);

for (u32 i = 0; i < ARCHAD_MAX_ROWS; i++) { selected[i] = 0; }

}
else
{

sound_play_error();

int s = 0;

const char* err_msg = "Echec (rien coche, ou > 64 Ko).";

while (err_msg[s] != 0) { status_line[s] = err_msg[s]; s++; }

status_line[s] = 0;

}

redraw = 1;

}

}
else /* ARCHAD_MODE_VIEW */
{


if (gui_point_in_rect(extract_btn.x, extract_btn.y, extract_btn.w, extract_btn.h, mx, my))
{

archad_extract_all(view_entries, view_count);

mode = ARCHAD_MODE_LIST;

status_line[0] = 0;

count = archad_scan(names, sizes, ARCHAD_MAX_ROWS);

for (u32 i = 0; i < ARCHAD_MAX_ROWS; i++) { selected[i] = 0; }

redraw = 1;

}

else if (gui_point_in_rect(back_btn.x, back_btn.y, back_btn.w, back_btn.h, mx, my))
{

mode = ARCHAD_MODE_LIST;

redraw = 1;

}

}

}


if (mode == ARCHAD_MODE_LIST && keyboard_available())
{

char c = keyboard_getchar();

u32 len = mk_strlen(archive_name);


if (c == '\b')
{

if (len > 0) { archive_name[len - 1] = 0; }

}

else if (c >= 32 && c < 127 && len < FILE_NAME_SIZE - 1)
{

archive_name[len] = c;

archive_name[len + 1] = 0;

}


gui_draw_field(win_x + 1, win_y + win_h - 8, win_w - 2, 2, archive_name, 0, 1, &name_field_box.x, &name_field_box.y, &name_field_box.w, &name_field_box.h);

}


unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{

}


}


}
