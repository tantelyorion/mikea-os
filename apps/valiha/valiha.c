#include "valiha.h"

#include "../../gui/gui.h"

#include "../../gui/theme.h"

#include "../../gui/icons.h"

#include "../../gui/desktop.h"

#include "../../gui/window.h"

#include "../../kernel/drivers/graphics/graphics.h"

#include "../../kernel/drivers/mouse/mouse.h"

#include "../../kernel/drivers/soundblaster/sb16.h"

#include "../../filesystem/inode.h"

#include "../../filesystem/file.h"

#include "../../filesystem/directory.h"

#include "../../libc/string.h"


void console_write(const char* text);
int keyboard_available();
char keyboard_getchar();
unsigned long timer_ticks();
void sound_play_error();


#define VALIHA_MAX_ROWS 12

#define GFX_SCALE 2


typedef struct
{

u32 x, y, w, h;

} valiha_hitbox;


/*
    Verifie qu'un nom se termine par ".wav" (comparaison
    simplifiee, minuscules attendues comme partout ailleurs
    dans ce noyau -- voir apps/archad/archad.c pour le meme
    principe applique a ".oar").
*/

static int valiha_has_wav_extension(const char* name)
{

u32 len = 0;

while (name[len] != 0) { len++; }


if (len < 4) { return 0; }


return name[len-4]=='.' && name[len-3]=='w' && name[len-2]=='a' && name[len-1]=='v';

}


/*
    Balaye la racine et ne retient que les fichiers ".wav" --
    meme principe que archad_scan() (apps/archad/archad.c).
*/

static u32 valiha_scan(char names[][FILE_NAME_SIZE], u32 max_count)
{

char raw_names[VALIHA_MAX_ROWS];

(void)raw_names;


char all_names[VALIHA_MAX_ROWS][FILE_NAME_SIZE];

int all_is_dir[VALIHA_MAX_ROWS];


u32 raw_count = directory_list_children("", all_names, all_is_dir, VALIHA_MAX_ROWS);


u32 count = 0;

for (u32 i = 0; i < raw_count && count < max_count; i++)
{

if (all_is_dir[i]) { continue; }

if (!valiha_has_wav_extension(all_names[i])) { continue; }


int j = 0;

while (all_names[i][j] != 0 && j < FILE_NAME_SIZE - 1) { names[count][j] = all_names[i][j]; j++; }

names[count][j] = 0;


count++;

}


return count;

}


/*
    Lit un u16/u32 petit-boyen a partir d'un tampon d'octets --
    ce noyau tourne en x86_64 (deja petit-boyen), mais lire
    explicitement octet par octet reste plus sur/lisible qu'un
    cast de pointeur direct sur un en-tete WAV externe dont
    l'alignement n'est pas garanti.
*/

static u16 read_u16le(const u8* p) { return (u16)(p[0] | (p[1] << 8)); }

static u32 read_u32le(const u8* p) { return (u32)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)); }


/*
    Analyse un fichier WAV (RIFF/PCM) deja charge dans "wav" (
    "wav_len" octets) et produit un tampon PCM 8 bits non
    signe, MONO dans "out_pcm" (capacite "out_capacity") -- le
    seul format que sait jouer la carte son (voir
    kernel/drivers/soundblaster/sb16.h). Renvoie la longueur du
    PCM produit (0 en cas d'erreur ou de format non gere --
    voir les cas ci-dessous), et ecrit la frequence
    d'echantillonnage trouvee dans "*out_rate".

    Formats geres : 8 ou 16 bits par echantillon, mono ou
    stereo (repli automatique vers du mono par moyenne des deux
    voies) -- couvre l'immense majorite des fichiers WAV
    "simples" rencontres en pratique. Tout le reste (24/32
    bits, PCM flottant, plus de deux voies...) est refuse
    proprement plutot que de produire un bruit incomprehensible.
*/

static u32 valiha_decode_wav(const u8* wav, u32 wav_len, u8* out_pcm, u32 out_capacity, u32* out_rate)
{

if (wav_len < 44) { return 0; }

if (wav[0]!='R' || wav[1]!='I' || wav[2]!='F' || wav[3]!='F') { return 0; }

if (wav[8]!='W' || wav[9]!='A' || wav[10]!='V' || wav[11]!='E') { return 0; }


u32 pos = 12;

u16 channels = 0;

u32 sample_rate = 0;

u16 bits_per_sample = 0;

const u8* data_ptr = (void*)0;

u32 data_len = 0;


while (pos + 8 <= wav_len)
{

char id0 = wav[pos], id1 = wav[pos+1], id2 = wav[pos+2], id3 = wav[pos+3];

u32 chunk_size = read_u32le(wav + pos + 4);

u32 chunk_data = pos + 8;


if (id0=='f' && id1=='m' && id2=='t' && id3==' ')
{

if (chunk_data + 16 > wav_len) { return 0; }

channels = read_u16le(wav + chunk_data + 2);

sample_rate = read_u32le(wav + chunk_data + 4);

bits_per_sample = read_u16le(wav + chunk_data + 14);

}

else if (id0=='d' && id1=='a' && id2=='t' && id3=='a')
{

data_ptr = wav + chunk_data;

data_len = chunk_size;

if (chunk_data + data_len > wav_len) { data_len = wav_len - chunk_data; }

}


pos = chunk_data + chunk_size + (chunk_size % 2);

}


if (data_ptr == (void*)0 || channels == 0 || sample_rate == 0) { return 0; }

if (bits_per_sample != 8 && bits_per_sample != 16) { return 0; }

if (channels != 1 && channels != 2) { return 0; }


u32 bytes_per_sample_frame = (bits_per_sample / 8) * channels;

if (bytes_per_sample_frame == 0) { return 0; }


u32 frame_count = data_len / bytes_per_sample_frame;

if (frame_count > out_capacity) { frame_count = out_capacity; }


for (u32 i = 0; i < frame_count; i++)
{

const u8* frame = data_ptr + i * bytes_per_sample_frame;

s32 mixed;


if (bits_per_sample == 8)
{

/* WAV 8 bits est deja NON SIGNE (convention du format) -- centre sur 128, comme notre PCM cible. */

if (channels == 1)
{

mixed = frame[0];

}
else
{

mixed = ((s32)frame[0] + (s32)frame[1]) / 2;

}

}
else
{

/*
    16 bits SIGNE, petit-boyen -- ramene sur 8 bits en gardant
    l'octet fort, puis recentre sur 128 (non signe). types.h ne
    definit pas de type 16 bits signe : extension de signe
    manuelle a partir de la valeur non signee lue.
*/

u16 raw0 = read_u16le(frame);

s32 s0 = (raw0 >= 32768) ? (s32)raw0 - 65536 : (s32)raw0;

if (channels == 1)
{

mixed = (s0 >> 8) + 128;

}
else
{

u16 raw1 = read_u16le(frame + 2);

s32 s1 = (raw1 >= 32768) ? (s32)raw1 - 65536 : (s32)raw1;

s32 avg16 = (s0 + s1) / 2;

mixed = (avg16 >> 8) + 128;

}

}


if (mixed < 0) { mixed = 0; }

if (mixed > 255) { mixed = 255; }


out_pcm[i] = (u8)mixed;

}


*out_rate = sample_rate;


return frame_count;

}


void cmd_valiha_app()
{


if (!gfx_available())
{

console_write("Valiha necessite le mode graphique.\n");

return;

}


int slot = gui_window_claim_slot();

if (slot < 0)
{

return;

}


int win_x = 3, win_y = 3, win_w = 30, win_h = 18;

int maximized = 0;

int saved_x = win_x, saved_y = win_y, saved_w = win_w, saved_h = win_h;


char names[VALIHA_MAX_ROWS][FILE_NAME_SIZE];

u32 count = valiha_scan(names, VALIHA_MAX_ROWS);


char now_playing[FILE_NAME_SIZE];

now_playing[0] = 0;


char status_line[40];

status_line[0] = 0;


static u8 g_wav_buffer[65536];

static u8 g_pcm_buffer[65536];


valiha_hitbox row[VALIHA_MAX_ROWS];

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


gui_draw_window(
win_x, win_y, win_w, win_h, "Valiha", theme_text_attr(),
&close_bx, &close_by, &close_bsize,
&max_bx, &max_by, &max_bsize, maximized,
&min_bx, &min_by, &min_bsize
);


gui_draw_text(win_x + 1, win_y + 1, "Fichiers .wav a la racine :", theme_text_attr());


for (u32 i = 0; i < count; i++)
{

icon_draw_music((u32)(win_x + 1) * 8 * GFX_SCALE, (u32)(win_y + 2 + (int)i) * 8 * GFX_SCALE, 8 * GFX_SCALE, theme_text());

gui_draw_button(win_x + 3, win_y + 2 + (int)i, win_w - 4, 1, names[i], &row[i].x, &row[i].y, &row[i].w, &row[i].h);

}


if (count == 0)
{

gui_draw_text(win_x + 1, win_y + 3, "(aucun fichier .wav a la racine)", theme_text_attr());

}


if (!sb16_available())
{

gui_draw_text(win_x + 1, win_y + win_h - 4, "Aucune carte son detectee.", theme_text_attr());

}

else if (now_playing[0] != 0)
{

gui_draw_text(win_x + 1, win_y + win_h - 4, "En lecture...", theme_text_attr());

gui_draw_text(win_x + 1, win_y + win_h - 3, now_playing, theme_text_attr());

}

else if (status_line[0] != 0)
{

gui_draw_text(win_x + 1, win_y + win_h - 3, status_line, theme_text_attr());

}
else
{

gui_draw_text(win_x + 1, win_y + win_h - 3, "Cliquez un fichier pour le jouer.", theme_text_attr());

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


for (u32 i = 0; i < count; i++)
{

if (gui_point_in_rect(row[i].x, row[i].y, row[i].w, row[i].h, mx, my))
{


int k = 0;

while (names[i][k] != 0 && k < FILE_NAME_SIZE - 1) { now_playing[k] = names[i][k]; k++; }

now_playing[k] = 0;

status_line[0] = 0;

redraw = 1;


/*
    Redessine IMMEDIATEMENT (avant la lecture, qui
    bloque -- voir sb16_play_pcm(),
    kernel/drivers/soundblaster/sb16.c) pour que
    "En lecture..." s'affiche pendant, pas seulement
    apres.
*/

desktop_render_backdrop();

gui_draw_window(win_x, win_y, win_w, win_h, "Valiha", theme_text_attr(), &close_bx, &close_by, &close_bsize, &max_bx, &max_by, &max_bsize, maximized, &min_bx, &min_by, &min_bsize);

gui_draw_text(win_x + 1, win_y + 1, "Fichiers .wav a la racine :", theme_text_attr());

for (u32 r = 0; r < count; r++) { icon_draw_music((u32)(win_x + 1) * 8 * GFX_SCALE, (u32)(win_y + 2 + (int)r) * 8 * GFX_SCALE, 8 * GFX_SCALE, theme_text()); gui_draw_button(win_x + 3, win_y + 2 + (int)r, win_w - 4, 1, names[r], &row[r].x, &row[r].y, &row[r].w, &row[r].h); }

gui_draw_text(win_x + 1, win_y + win_h - 4, "En lecture...", theme_text_attr());

gui_draw_text(win_x + 1, win_y + win_h - 3, now_playing, theme_text_attr());


u32 wav_len = file_read_bin(names[i], g_wav_buffer, sizeof(g_wav_buffer));

u32 rate = 0;

u32 pcm_len = valiha_decode_wav(g_wav_buffer, wav_len, g_pcm_buffer, sizeof(g_pcm_buffer), &rate);


if (pcm_len == 0 || !sb16_available())
{

sound_play_error();

int s = 0;

const char* err_msg = "Lecture impossible (format non gere ?).";

while (err_msg[s] != 0) { status_line[s] = err_msg[s]; s++; }

status_line[s] = 0;

}
else
{

sb16_play_pcm(g_pcm_buffer, pcm_len, rate);

}


now_playing[0] = 0;

redraw = 1;


break;

}

}

}


if (keyboard_available())
{

keyboard_getchar();

}


unsigned long frame_start = timer_ticks();

while (timer_ticks() - frame_start < 1)
{

}


}


}
