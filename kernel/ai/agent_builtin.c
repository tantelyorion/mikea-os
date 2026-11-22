#include "agent_builtin.h"

static const agent_platform* P = (const agent_platform*)0;

/* ------------------------------------------------------------
   Texte
   ------------------------------------------------------------ */

static u32 b_len(const char* s) { u32 n = 0; while (s[n] != 0) { n++; } return n; }

static int b_eq(const char* a, const char* b)
{
    u32 i = 0;

    while (a[i] != 0 && a[i] == b[i]) { i++; }

    return a[i] == b[i];
}

static void b_copy(char* d, u32 max, const char* s)
{
    u32 i = 0;

    if (max == 0) { return; }

    while (s[i] != 0 && i < max - 1) { d[i] = s[i]; i++; }

    d[i] = 0;
}

static void b_add(char* d, u32 max, const char* s)
{
    u32 n = b_len(d);

    for (u32 i = 0; s[i] != 0 && n < max - 1; i++) { d[n++] = s[i]; }

    d[n] = 0;
}

/* Minuscules ASCII dans un tampon local (les valeurs attendues sont ASCII). */

static void b_lower(const char* src, char* dst, u32 max)
{
    u32 i = 0;

    for (; src[i] != 0 && i < max - 1; i++)
    {
        dst[i] = (src[i] >= 'A' && src[i] <= 'Z') ? (char)(src[i] + 32) : src[i];
    }

    dst[i] = 0;
}

static u32 b_parse_small(const char* s)      /* entier 0..99, 0xFFFFFFFF si invalide */
{
    if (s[0] < '0' || s[0] > '9') { return 0xFFFFFFFFu; }

    u32 v = (u32)(s[0] - '0');

    if (s[1] != 0)
    {
        if (s[1] < '0' || s[1] > '9' || s[2] != 0) { return 0xFFFFFFFFu; }

        v = v * 10 + (u32)(s[1] - '0');
    }

    return v;
}

static void b_add_u32(char* d, u32 max, u32 v)
{
    char tmp[12];
    int t = 0;

    if (v == 0) { tmp[t++] = '0'; }

    while (v > 0 && t < 11) { tmp[t++] = (char)('0' + v % 10); v /= 10; }

    char out[12];
    int o = 0;

    while (t > 0) { out[o++] = tmp[--t]; }

    out[o] = 0;
    b_add(d, max, out);
}

/* ------------------------------------------------------------
   Validation des arguments
   ------------------------------------------------------------ */

static int v_open_app(const char* a1, const char* a2)
{
    (void)a2;

    return agent_resolve_app(a1) != (const char*)0;
}

/* Valeur parmi une liste ; renvoie l'indice de la liste, -1 sinon. */

static int pick(const char* a1, const char* const* list)
{
    char low[24];

    b_lower(a1, low, sizeof(low));

    for (u32 i = 0; list[i] != (const char*)0; i++)
    {
        if (b_eq(low, list[i])) { return (int)i; }
    }

    return -1;
}

static const char* const THEME_DARK[]  = { "sombre", "dark", "nuit", "noir", (const char*)0 };
static const char* const THEME_LIGHT[] = { "clair", "light", "jour", "blanc", (const char*)0 };
static const char* const THEME_TOGGLE[] = { "basculer", "toggle", "inverser", "changer", (const char*)0 };
static const char* const SND_OFF[] = { "off", "non", "coupe", "couper", "mute", "desactive", "desactiver", "0", "false", (const char*)0 };
static const char* const SND_ON[]  = { "on", "oui", "active", "activer", "unmute", "1", "true", (const char*)0 };
static const char* const WP_NEXT[] = { "suivant", "next", "autre", "prochain", (const char*)0 };

static int v_set_theme(const char* a1, const char* a2)
{
    (void)a2;

    return pick(a1, THEME_DARK) >= 0 || pick(a1, THEME_LIGHT) >= 0 || pick(a1, THEME_TOGGLE) >= 0;
}

static int v_set_sound(const char* a1, const char* a2)
{
    (void)a2;

    return pick(a1, SND_OFF) >= 0 || pick(a1, SND_ON) >= 0;
}

static int v_set_wallpaper(const char* a1, const char* a2)
{
    (void)a2;

    if (pick(a1, WP_NEXT) >= 0) { return 1; }

    u32 n = b_parse_small(a1);

    return n != 0xFFFFFFFFu && n >= 1 && P != (const agent_platform*)0 && n <= P->wallpaper_count();
}

static const char* const INFO_KINDS[] = { "tout", "memoire", "disque", "heure", (const char*)0 };

static int v_system_info(const char* a1, const char* a2)
{
    (void)a2;

    return a1[0] == 0 || pick(a1, INFO_KINDS) >= 0;
}

static int v_dir(const char* a1, const char* a2)
{
    (void)a2;

    return a1[0] == 0 || agent_valid_filename(a1);
}

static int v_file(const char* a1, const char* a2)
{
    (void)a2;

    return agent_valid_filename(a1);
}

static int v_create(const char* a1, const char* a2)
{
    return agent_valid_filename(a1) && b_len(a2) <= 120;
}

static int v_none(const char* a1, const char* a2)
{
    (void)a1;
    (void)a2;

    return 1;
}

/* ------------------------------------------------------------
   Actions
   ------------------------------------------------------------ */

static int run_open_app(const char* a1, const char* a2, char* out, u32 max, agent_undo_info* u)
{
    (void)a2;
    (void)u;

    const char* app = agent_resolve_app(a1);

    if (app == (const char*)0 || !P->open_app(app))
    {
        b_copy(out, max, "Je n'ai pas pu ouvrir cette application.");
        return 0;
    }

    b_copy(out, max, "J'ouvre « ");
    b_add(out, max, app);
    b_add(out, max, " ».");

    return 1;
}

static int run_set_theme(const char* a1, const char* a2, char* out, u32 max, agent_undo_info* u)
{
    (void)a2;

    int was_dark = P->get_dark();
    int want = was_dark;

    if (pick(a1, THEME_DARK) >= 0) { want = 1; }
    else if (pick(a1, THEME_LIGHT) >= 0) { want = 0; }
    else { want = !was_dark; }

    u->data[0][0] = was_dark ? '1' : '0';

    if (want == was_dark)
    {
        b_copy(out, max, want ? "Le mode sombre est déjà actif." : "Le mode clair est déjà actif.");
        return 1;
    }

    P->set_dark(want);
    b_copy(out, max, want ? "Mode sombre activé." : "Mode clair activé.");

    return 1;
}

static int undo_theme(const agent_undo_info* u, char* out, u32 max)
{
    P->set_dark(u->data[0][0] == '1');
    b_copy(out, max, "Thème rétabli.");

    return 1;
}

static int run_set_sound(const char* a1, const char* a2, char* out, u32 max, agent_undo_info* u)
{
    (void)a2;

    int was_on = P->get_sound();
    int want = (pick(a1, SND_ON) >= 0);

    u->data[0][0] = was_on ? '1' : '0';

    P->set_sound(want);
    b_copy(out, max, want ? "Son activé." : "Son coupé.");

    return 1;
}

static int undo_sound(const agent_undo_info* u, char* out, u32 max)
{
    P->set_sound(u->data[0][0] == '1');
    b_copy(out, max, "Son rétabli.");

    return 1;
}

static int run_set_wallpaper(const char* a1, const char* a2, char* out, u32 max, agent_undo_info* u)
{
    (void)a2;

    u32 count = P->wallpaper_count();
    u32 was = P->get_wallpaper();
    u32 want;

    if (pick(a1, WP_NEXT) >= 0) { want = (count > 0) ? (was + 1) % count : 0; }
    else { want = b_parse_small(a1) - 1; }

    u->data[0][0] = 0;
    b_add_u32(u->data[0], sizeof(u->data[0]), was);

    P->set_wallpaper(want);

    b_copy(out, max, "Fond d’écran n° ");
    b_add_u32(out, max, want + 1);
    b_add(out, max, " appliqué.");

    return 1;
}

static int undo_wallpaper(const agent_undo_info* u, char* out, u32 max)
{
    P->set_wallpaper(b_parse_small(u->data[0]) == 0xFFFFFFFFu ? 0 : b_parse_small(u->data[0]));
    b_copy(out, max, "Fond d’écran rétabli.");

    return 1;
}

static int run_system_info(const char* a1, const char* a2, char* out, u32 max, agent_undo_info* u)
{
    (void)a2;
    (void)u;

    char low[24];

    b_lower(a1[0] ? a1 : "tout", low, sizeof(low));
    P->sysinfo(low, out, max);

    return 1;
}

static int run_list_files(const char* a1, const char* a2, char* out, u32 max, agent_undo_info* u)
{
    (void)a2;
    (void)u;

    char names[160];
    names[0] = 0;

    u32 n = P->list_files(a1, names, sizeof(names));

    if (n == 0)
    {
        b_copy(out, max, a1[0] ? "Ce dossier est vide (ou n'existe pas)." : "Aucun fichier.");
        return 1;
    }

    out[0] = 0;
    b_add_u32(out, max, n);
    b_add(out, max, n == 1 ? " élément : " : " éléments : ");
    b_add(out, max, names);

    return 1;
}

static int run_read_file(const char* a1, const char* a2, char* out, u32 max, agent_undo_info* u)
{
    (void)a2;
    (void)u;

    char text[160];

    if (!P->file_exists(a1) || !P->read_file(a1, text, sizeof(text)))
    {
        b_copy(out, max, "Je ne trouve pas ce fichier.");
        return 0;
    }

    b_copy(out, max, a1);
    b_add(out, max, " : ");
    b_add(out, max, text[0] ? text : "(vide)");

    return 1;
}

static int run_create_file(const char* a1, const char* a2, char* out, u32 max, agent_undo_info* u)
{
    if (P->file_exists(a1))
    {
        b_copy(out, max, "Ce fichier existe déjà : je ne l'écrase pas.");
        return 0;
    }

    if (!P->create_file(a1, a2))
    {
        b_copy(out, max, "Impossible de créer ce fichier.");
        return 0;
    }

    b_copy(u->data[0], sizeof(u->data[0]), a1);
    b_copy(out, max, "Fichier « ");
    b_add(out, max, a1);
    b_add(out, max, " » créé.");

    return 1;
}

static int undo_create(const agent_undo_info* u, char* out, u32 max)
{
    int ok = P->trash_file(u->data[0]);

    b_copy(out, max, ok ? "Fichier mis à la corbeille." : "Impossible d'annuler la création.");

    return ok;
}

static int run_delete_file(const char* a1, const char* a2, char* out, u32 max, agent_undo_info* u)
{
    (void)a2;

    if (!P->file_exists(a1))
    {
        b_copy(out, max, "Je ne trouve pas ce fichier.");
        return 0;
    }

    if (!P->trash_file(a1))
    {
        b_copy(out, max, "Impossible de mettre ce fichier à la corbeille.");
        return 0;
    }

    b_copy(u->data[0], sizeof(u->data[0]), a1);
    b_copy(out, max, "« ");
    b_add(out, max, a1);
    b_add(out, max, " » est à la corbeille (récupérable).");

    return 1;
}

static int undo_delete(const agent_undo_info* u, char* out, u32 max)
{
    int ok = P->restore_file(u->data[0]);

    b_copy(out, max, ok ? "Fichier restauré." : "Impossible de restaurer ce fichier.");

    return ok;
}

static int run_restore_file(const char* a1, const char* a2, char* out, u32 max, agent_undo_info* u)
{
    (void)a2;

    if (!P->restore_file(a1))
    {
        b_copy(out, max, "Ce fichier n'est pas dans la corbeille.");
        return 0;
    }

    b_copy(u->data[0], sizeof(u->data[0]), a1);
    b_copy(out, max, "« ");
    b_add(out, max, a1);
    b_add(out, max, " » restauré.");

    return 1;
}

static int undo_restore(const agent_undo_info* u, char* out, u32 max)
{
    int ok = P->trash_file(u->data[0]);

    b_copy(out, max, ok ? "Fichier remis à la corbeille." : "Impossible d'annuler.");

    return ok;
}

static int run_logout(const char* a1, const char* a2, char* out, u32 max, agent_undo_info* u)
{
    (void)a1;
    (void)a2;
    (void)u;

    P->logout();
    b_copy(out, max, "Fermeture de la session...");

    return 1;
}

/* ------------------------------------------------------------
   Registre
   ------------------------------------------------------------ */

static const agent_action BUILTIN[] = {
    { "open_app", "ouvre une application. arg1 = fichiers, calculatrice, parametres, terminal, corbeille, valiha, archad ou compte.",
      AGENT_PERM_REVERSIBLE, (const char*)0, v_open_app, run_open_app, (int (*)(const agent_undo_info*, char*, u32))0 },

    { "set_theme", "change le theme. arg1 = sombre, clair ou basculer.",
      AGENT_PERM_REVERSIBLE, (const char*)0, v_set_theme, run_set_theme, undo_theme },

    { "set_sound", "active ou coupe le son du systeme. arg1 = on ou off.",
      AGENT_PERM_REVERSIBLE, (const char*)0, v_set_sound, run_set_sound, undo_sound },

    { "set_wallpaper", "change le fond d'ecran. arg1 = numero de 1 a 6, ou suivant.",
      AGENT_PERM_REVERSIBLE, (const char*)0, v_set_wallpaper, run_set_wallpaper, undo_wallpaper },

    { "system_info", "donne des infos systeme. arg1 = memoire, disque, heure ou tout.",
      AGENT_PERM_READ, (const char*)0, v_system_info, run_system_info, (int (*)(const agent_undo_info*, char*, u32))0 },

    { "list_files", "liste les fichiers. arg1 = nom d'un dossier, ou vide pour la racine.",
      AGENT_PERM_READ, (const char*)0, v_dir, run_list_files, (int (*)(const agent_undo_info*, char*, u32))0 },

    { "read_file", "lit le debut d'un fichier. arg1 = nom du fichier.",
      AGENT_PERM_READ, (const char*)0, v_file, run_read_file, (int (*)(const agent_undo_info*, char*, u32))0 },

    { "create_file", "cree un fichier. arg1 = nom du fichier (ex. notes.txt), arg2 = contenu texte (peut etre vide).",
      AGENT_PERM_WRITE, "Créer le fichier « %1 » ?", v_create, run_create_file, undo_create },

    { "restore_file", "sort un fichier de la corbeille. arg1 = nom du fichier.",
      AGENT_PERM_WRITE, "Restaurer « %1 » depuis la corbeille ?", v_file, run_restore_file, undo_restore },

    { "delete_file", "met un fichier a la corbeille (recuperable). arg1 = nom du fichier.",
      AGENT_PERM_DESTRUCTIVE, "Mettre « %1 » à la corbeille ?", v_file, run_delete_file, undo_delete },

    { "logout", "ferme la session de l'utilisateur.",
      AGENT_PERM_DESTRUCTIVE, "Fermer votre session ?", v_none, run_logout, (int (*)(const agent_undo_info*, char*, u32))0 },
};

void agent_builtin_install(const agent_platform* platform)
{
    P = platform;
    agent_set_registry(BUILTIN, (u32)(sizeof(BUILTIN) / sizeof(BUILTIN[0])));
}
