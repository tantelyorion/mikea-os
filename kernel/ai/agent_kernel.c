#include "agent_kernel.h"
#include "agent_builtin.h"
#include "jarvis.h"

#include "../../gui/theme.h"
#include "../../gui/window.h"
#include "../../gui/assets/wallpaper_images.h"
#include "../drivers/speaker/speaker.h"
#include "../drivers/rtc/rtc.h"
#include "../memory/heap.h"
#include "../../filesystem/block.h"
#include "../../filesystem/file.h"
#include "../../filesystem/directory.h"
#include "../../shell/msh.h"

void cmd_files();
void cmd_calc();
void cmd_settings();
void cmd_gui();
void cmd_trash_app();
void cmd_terminal_app();
void cmd_archad_app();
void cmd_valiha_app();

/* ------------------------------------------------------------
   Texte
   ------------------------------------------------------------ */

static void k_add(char* d, u32 max, const char* s)
{
    u32 n = 0;

    while (d[n] != 0) { n++; }

    for (u32 i = 0; s[i] != 0 && n < max - 1; i++) { d[n++] = s[i]; }

    d[n] = 0;
}

static void k_add_u32(char* d, u32 max, u32 v, int pad2)
{
    char tmp[12];
    int t = 0;

    if (v == 0) { tmp[t++] = '0'; }

    while (v > 0 && t < 11) { tmp[t++] = (char)('0' + v % 10); v /= 10; }

    if (pad2 && t < 2) { tmp[t++] = '0'; }

    char out[12];
    int o = 0;

    while (t > 0) { out[o++] = tmp[--t]; }

    out[o] = 0;
    k_add(d, max, out);
}

/* Les fonctions de fichiers prennent des "char*" non constants : copie locale. */

static void name_copy(const char* src, char* dst, u32 max)
{
    u32 i = 0;

    for (; src[i] != 0 && i < max - 1; i++) { dst[i] = src[i]; }

    dst[i] = 0;
}

/* ------------------------------------------------------------
   Plateforme reelle
   ------------------------------------------------------------ */

typedef struct
{
    const char* canonical;
    const char* label;
    void (*entry)();
} app_entry;

/* L'installeur et les actions du centre d'applications (redemarrer...) ne sont PAS ouvrables par l'agent. */

static const app_entry APPS[] = {
    { "fichiers",     "Fichiers",     cmd_files },
    { "calculatrice", "Calculatrice", cmd_calc },
    { "parametres",   "Parametres",   cmd_settings },
    { "compte",       "Compte",       cmd_gui },
    { "corbeille",    "Corbeille",    cmd_trash_app },
    { "terminal",     "Terminal",     cmd_terminal_app },
    { "archad",       "Archad",       cmd_archad_app },
    { "valiha",       "Valiha",       cmd_valiha_app },
};

static int g_opened_slot = -1;

static int kp_open_app(const char* canonical)
{
    for (u32 i = 0; i < sizeof(APPS) / sizeof(APPS[0]); i++)
    {
        const char* a = canonical;
        const char* b = APPS[i].canonical;

        while (*a != 0 && *a == *b) { a++; b++; }

        if (*a != 0 || *b != 0) { continue; }

        /* Deja ouverte : on la ramene au premier plan au lieu d'en creer une seconde. */

        int slot = gui_window_find_by_entry(APPS[i].entry);

        if (slot >= 0)
        {
            gui_window_focus(slot);
            g_opened_slot = slot;
            return 1;
        }

        slot = gui_window_open(APPS[i].label, APPS[i].entry);

        if (slot < 0) { return 0; }

        g_opened_slot = slot;
        return 1;
    }

    return 0;
}

static int  kp_get_dark(void)          { return theme_is_dark(); }
static void kp_set_dark(int dark)      { theme_set_dark(dark); }
static int  kp_get_sound(void)         { return sound_is_enabled(); }
static void kp_set_sound(int on)       { sound_set_enabled(on); }
static u32  kp_wallpaper_count(void)   { return wallpaper_photo_count(); }
static u32  kp_get_wallpaper(void)     { return (u32)wallpaper_get_photo_index(); }
static void kp_set_wallpaper(u32 i)    { wallpaper_set_photo_index((int)i); }

static void kp_sysinfo(const char* what, char* out, u32 max)
{
    out[0] = 0;

    int all = (what[0] == 't');   /* "tout" */
    int mem = all || (what[0] == 'm');
    int dsk = all || (what[0] == 'd');
    int tim = all || (what[0] == 'h');

    if (mem)
    {
        u32 used, total;
        heap_stats(&used, &total);

        k_add(out, max, "Mémoire : ");
        k_add_u32(out, max, used / 1024, 0);
        k_add(out, max, " / ");
        k_add_u32(out, max, total / 1024, 0);
        k_add(out, max, " Ko utilisés. ");
    }

    if (dsk)
    {
        u32 free_blocks = 0;

        for (u32 b = 0; b < TOTAL_BLOCKS; b++)
        {
            if (block_is_free(b)) { free_blocks++; }
        }

        k_add(out, max, "Disque : ");
        k_add_u32(out, max, (free_blocks * BLOCK_SIZE) / 1024, 0);
        k_add(out, max, " Ko libres sur ");
        k_add_u32(out, max, (TOTAL_BLOCKS * BLOCK_SIZE) / 1024, 0);
        k_add(out, max, " Ko. ");
    }

    if (tim)
    {
        rtc_time now;
        rtc_read(&now);

        k_add(out, max, "Il est ");
        k_add_u32(out, max, now.hour, 1);
        k_add(out, max, ":");
        k_add_u32(out, max, now.minute, 1);
        k_add(out, max, ", le ");
        k_add_u32(out, max, now.day, 1);
        k_add(out, max, "/");
        k_add_u32(out, max, now.month, 1);
        k_add(out, max, "/");
        k_add_u32(out, max, now.year, 0);
        k_add(out, max, ".");
    }
}

static u32 kp_list_files(const char* dir, char* out, u32 max)
{
    char parent[FILE_NAME_SIZE];
    char names[24][FILE_NAME_SIZE];
    int is_dir[24];

    name_copy(dir, parent, sizeof(parent));
    out[0] = 0;

    u32 n = directory_list_children(parent, names, is_dir, 24);

    for (u32 i = 0; i < n; i++)
    {
        if (i > 0) { k_add(out, max, ", "); }

        k_add(out, max, names[i]);

        if (is_dir[i]) { k_add(out, max, "/"); }
    }

    return n;
}

static int kp_read_file(const char* name, char* out, u32 max)
{
    char n[FILE_NAME_SIZE];

    name_copy(name, n, sizeof(n));

    char* content = file_read(n);

    if (content == (char*)0) { return 0; }

    /* Tampon statique de 512 octets, pas forcement termine : on borne et on aplatit. */

    u32 i = 0;

    for (; content[i] != 0 && i < 480 && i < max - 1; i++)
    {
        unsigned char c = (unsigned char)content[i];
        out[i] = (c == '\n' || c == '\r' || c == '\t') ? ' ' : (c < 32 ? '?' : (char)c);
    }

    out[i] = 0;

    return 1;
}

static int kp_file_exists(const char* name)
{
    char n[FILE_NAME_SIZE];

    name_copy(name, n, sizeof(n));

    return file_exists(n);   /* un fichier a la corbeille est vu comme absent */
}

static int kp_create_file(const char* name, const char* content)
{
    char n[FILE_NAME_SIZE];
    char c[128];

    name_copy(name, n, sizeof(n));
    name_copy(content, c, sizeof(c));

    if (!file_create(n)) { return 0; }

    if (c[0] != 0 && !file_write(n, c))
    {
        file_trash(n);   /* tout ou rien : pas de fichier a moitie cree */
        return 0;
    }

    return 1;
}

static int kp_trash_file(const char* name)
{
    char n[FILE_NAME_SIZE];

    name_copy(name, n, sizeof(n));

    return file_trash(n);
}

static int kp_restore_file(const char* name)
{
    char n[FILE_NAME_SIZE];

    name_copy(name, n, sizeof(n));

    return file_restore(n);
}

static void kp_logout(void)
{
    shell_request_logout();
}

static const agent_platform PLATFORM = {
    kp_open_app, kp_get_dark, kp_set_dark, kp_get_sound, kp_set_sound,
    kp_wallpaper_count, kp_get_wallpaper, kp_set_wallpaper, kp_sysinfo,
    kp_list_files, kp_read_file, kp_file_exists, kp_create_file,
    kp_trash_file, kp_restore_file, kp_logout
};

static int g_ready = 0;

void agent_kernel_init(void)
{
    if (g_ready) { return; }

    agent_builtin_install(&PLATFORM);
    g_ready = 1;
}

int agent_kernel_take_opened_slot(void)
{
    int s = g_opened_slot;

    g_opened_slot = -1;

    return s;
}

/* ------------------------------------------------------------
   Cerveau : regles, puis modele local
   ------------------------------------------------------------ */

int agent_kernel_plan(const char* text, agent_call* out, int* used_llm)
{
    static char system_prompt[2100];
    static char schema[640];
    static char answer[420];

    *used_llm = 0;
    agent_kernel_init();

    /* Texte vide ou fait seulement d'espaces. */

    int blank = 1;

    for (u32 i = 0; text[i] != 0; i++) { if (text[i] != ' ' && text[i] != '\t') { blank = 0; } }

    if (blank) { return 0; }

    /* 1) Regles : instantane, hors ligne. */

    if (agent_plan_rules(text, out)) { return 1; }

    /* 2) Modele local, sortie contrainte par schema. */

    out->action = AGENT_ACT_REPLY;
    out->arg1[0] = 0;
    out->arg2[0] = 0;
    out->source = AGENT_SRC_LLM;

    if (!jarvis_llm_available())
    {
        name_copy("Je n'ai pas compris, et aucun modèle IA local n'est joignable. "
                  "Les commandes simples marchent hors ligne : tapez « aide ».", out->say, sizeof(out->say));
        return 1;
    }

    agent_build_system_prompt(system_prompt, sizeof(system_prompt));
    agent_build_schema(schema, sizeof(schema));

    *used_llm = 1;

    answer[0] = 0;

    if (!jarvis_llm_generate(system_prompt, text, schema, answer, sizeof(answer)))
    {
        name_copy("Le modèle IA local ne répond pas. Lancez Ollama sur votre PC (« ollama serve »).",
                  out->say, sizeof(out->say));
        return 1;
    }

    if (agent_parse_json(answer, out)) { return 1; }

    /* Reponse non exploitable : on la montre telle quelle plutot que d'agir a l'aveugle. */

    out->action = AGENT_ACT_REPLY;
    name_copy(answer[0] ? answer : "Je n'ai pas su répondre.", out->say, sizeof(out->say));

    return 1;
}
