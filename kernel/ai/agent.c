#include "agent.h"

/*
    Cerveau de l'agent : voir agent.h. Aucune dependance au noyau
    (pas de libc : toutes les aides de texte sont locales).
*/

/* ------------------------------------------------------------
   Texte
   ------------------------------------------------------------ */

static u32 a_len(const char* s)
{
    u32 n = 0;

    while (s[n] != 0) { n++; }

    return n;
}

static int a_eq(const char* a, const char* b)
{
    u32 i = 0;

    while (a[i] != 0 && a[i] == b[i]) { i++; }

    return a[i] == b[i];
}

static void a_copy(char* dst, u32 max, const char* src)
{
    u32 i = 0;

    if (max == 0) { return; }

    while (src[i] != 0 && i < max - 1) { dst[i] = src[i]; i++; }

    dst[i] = 0;
}

static void a_add(char* dst, u32 max, const char* src)
{
    u32 n = a_len(dst);

    for (u32 i = 0; src[i] != 0 && n < max - 1; i++) { dst[n++] = src[i]; }

    dst[n] = 0;
}

/* ------------------------------------------------------------
   Registre
   ------------------------------------------------------------ */

static const agent_action* g_reg = (const agent_action*)0;
static u32 g_count = 0;

void agent_set_registry(const agent_action* table, u32 count)
{
    g_reg = table;
    g_count = count;
}

u32 agent_action_count(void)
{
    return g_count;
}

const agent_action* agent_action_at(u32 index)
{
    return (index < g_count) ? &g_reg[index] : (const agent_action*)0;
}

int agent_find(const char* name)
{
    for (u32 i = 0; i < g_count; i++)
    {
        if (a_eq(g_reg[i].name, name)) { return (int)i; }
    }

    return AGENT_ACT_NONE;
}

int agent_valid_filename(const char* name)
{
    u32 n = a_len(name);

    if (n == 0 || n > 31 || name[0] == '.') { return 0; }

    for (u32 i = 0; i < n; i++)
    {
        char c = name[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                 || c == '.' || c == '_' || c == '-';

        if (!ok) { return 0; }
        if (c == '.' && name[i + 1] == '.') { return 0; }
    }

    return 1;
}

/* ------------------------------------------------------------
   Journal et annulation
   ------------------------------------------------------------ */

#define LOG_SIZE  16
#define UNDO_SIZE 8

static agent_log_entry g_log[LOG_SIZE];
static u32 g_log_total = 0;               /* nombre total d'entrees ecrites */

typedef struct
{
    int action;
    u32 log_seq;
    agent_undo_info info;
} undo_record;

static undo_record g_undo[UNDO_SIZE];
static u32 g_undo_n = 0;

u32 agent_log_count(void)
{
    return g_log_total < LOG_SIZE ? g_log_total : LOG_SIZE;
}

const agent_log_entry* agent_log_at(u32 index)
{
    if (index >= agent_log_count()) { return (const agent_log_entry*)0; }

    return &g_log[(g_log_total - 1 - index) % LOG_SIZE];
}

static u32 log_push(const agent_call* c, int ok)
{
    agent_log_entry* e = &g_log[g_log_total % LOG_SIZE];

    g_log_total++;

    e->seq = g_log_total;
    e->action = c->action;
    a_copy(e->arg1, sizeof(e->arg1), c->arg1);
    e->ok = ok;
    e->undone = 0;
    e->source = c->source;

    return e->seq;
}

static void log_mark_undone(u32 seq)
{
    for (u32 i = 0; i < agent_log_count(); i++)
    {
        agent_log_entry* e = &g_log[(g_log_total - 1 - i) % LOG_SIZE];

        if (e->seq == seq) { e->undone = 1; return; }
    }
}

int agent_undo_available(void)
{
    return g_undo_n > 0;
}

int agent_undo_last(char* out, u32 outmax)
{
    out[0] = 0;

    if (g_undo_n == 0)
    {
        a_copy(out, outmax, "Il n'y a rien à annuler.");
        return 0;
    }

    undo_record* r = &g_undo[--g_undo_n];
    const agent_action* a = &g_reg[r->action];

    int ok = (a->undo != 0) ? a->undo(&r->info, out, outmax) : 0;

    if (ok) { log_mark_undone(r->log_seq); }

    return ok;
}

static void undo_push(int action, u32 log_seq, const agent_undo_info* info)
{
    if (g_undo_n == UNDO_SIZE)
    {
        for (u32 i = 1; i < UNDO_SIZE; i++) { g_undo[i - 1] = g_undo[i]; }

        g_undo_n--;
    }

    g_undo[g_undo_n].action = action;
    g_undo[g_undo_n].log_seq = log_seq;
    g_undo[g_undo_n].info = *info;
    g_undo_n++;
}

/* ------------------------------------------------------------
   Garde-fous
   ------------------------------------------------------------ */

static int arg_clean(const char* s, int allow_newline)
{
    for (u32 i = 0; s[i] != 0; i++)
    {
        unsigned char c = (unsigned char)s[i];

        if (c < 0x20 && !(allow_newline && (c == '\n' || c == '\t'))) { return 0; }
    }

    return 1;
}

agent_decision agent_decide(const agent_call* c, char* reason, u32 reason_max)
{
    reason[0] = 0;

    if (c->action == AGENT_ACT_REPLY || c->action == AGENT_ACT_UNDO)
    {
        return AGENT_DECISION_RUN;
    }

    if (c->action < 0 || (u32)c->action >= g_count)
    {
        a_copy(reason, reason_max, "Action inconnue : je ne peux pas faire ça.");
        return AGENT_DECISION_REJECT;
    }

    const agent_action* a = &g_reg[c->action];

    if (!arg_clean(c->arg1, 0) || !arg_clean(c->arg2, 1))
    {
        a_copy(reason, reason_max, "Arguments invalides (caractères interdits).");
        return AGENT_DECISION_REJECT;
    }

    if (a->validate != 0 && !a->validate(c->arg1, c->arg2))
    {
        a_copy(reason, reason_max, "Je n'ai pas compris les paramètres de cette action.");
        return AGENT_DECISION_REJECT;
    }

    switch (a->perm)
    {
        case AGENT_PERM_READ:
        case AGENT_PERM_REVERSIBLE:   return AGENT_DECISION_RUN;
        case AGENT_PERM_WRITE:        return AGENT_DECISION_CONFIRM;
        default:                      return AGENT_DECISION_CONFIRM_STRONG;
    }
}

void agent_describe(const agent_call* c, char* out, u32 max)
{
    out[0] = 0;

    if (c->action < 0 || (u32)c->action >= g_count)
    {
        a_copy(out, max, "Action inconnue ?");
        return;
    }

    const agent_action* a = &g_reg[c->action];

    if (a->confirm == (const char*)0)
    {
        a_copy(out, max, "Exécuter « ");
        a_add(out, max, a->name);
        a_add(out, max, " » ?");
        return;
    }

    for (u32 i = 0; a->confirm[i] != 0; i++)
    {
        if (a->confirm[i] == '%' && (a->confirm[i + 1] == '1' || a->confirm[i + 1] == '2'))
        {
            a_add(out, max, a->confirm[i + 1] == '1' ? c->arg1 : c->arg2);
            i++;
        }
        else
        {
            char one[2] = { a->confirm[i], 0 };
            a_add(out, max, one);
        }
    }
}

int agent_execute(const agent_call* c, int confirmed, char* out, u32 outmax)
{
    char reason[96];

    out[0] = 0;

    if (c->action == AGENT_ACT_REPLY)
    {
        a_copy(out, outmax, c->say);
        return 1;
    }

    if (c->action == AGENT_ACT_UNDO)
    {
        return agent_undo_last(out, outmax);
    }

    agent_decision d = agent_decide(c, reason, sizeof(reason));

    if (d == AGENT_DECISION_REJECT)
    {
        a_copy(out, outmax, reason);
        return 0;
    }

    if (d != AGENT_DECISION_RUN && !confirmed)
    {
        a_copy(out, outmax, "Action sensible : elle demande votre confirmation.");
        return 0;
    }

    const agent_action* a = &g_reg[c->action];
    agent_undo_info info;

    for (u32 k = 0; k < 2; k++) { for (u32 j = 0; j < 64; j++) { info.data[k][j] = 0; } }

    int ok = a->run(c->arg1, c->arg2, out, outmax, &info);
    u32 seq = log_push(c, ok);

    if (ok && a->undo != 0)
    {
        undo_push(c->action, seq, &info);
    }

    return ok;
}

/* ------------------------------------------------------------
   Sortie d'un modele : lecture tolerante du JSON
   ------------------------------------------------------------ */

static char lower_ascii(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static int key_in(const char* key, const char* const* list)
{
    for (u32 i = 0; list[i] != (const char*)0; i++)
    {
        if (a_eq(key, list[i])) { return 1; }
    }

    return 0;
}

static const char* const KEYS_ACTION[] = { "action", "tool", "function", "command", (const char*)0 };
static const char* const KEYS_ARG1[]   = { "arg1", "app", "name", "file", "filename", "path", "target", "value", "mode", "theme", "query", (const char*)0 };
static const char* const KEYS_ARG2[]   = { "arg2", "content", "body", "data", "text", (const char*)0 };
static const char* const KEYS_SAY[]    = { "say", "reply", "answer", "response", "message", (const char*)0 };

/* Lit une chaine JSON a partir du guillemet ouvrant ; renvoie l'index apres le guillemet fermant. */

static u32 json_string(const char* t, u32 i, char* out, u32 max)
{
    u32 n = 0;

    i++;   /* guillemet ouvrant */

    while (t[i] != 0 && t[i] != '"')
    {
        char c = t[i];

        if (c == '\\' && t[i + 1] != 0)
        {
            char e = t[i + 1];
            i += 2;

            if (e == 'n') { c = '\n'; }
            else if (e == 't') { c = '\t'; }
            else if (e == 'u')
            {
                /* \uXXXX : on ne garde que l'ASCII, le reste devient '?'. */
                u32 v = 0;

                for (u32 k = 0; k < 4 && t[i] != 0; k++, i++)
                {
                    char h = t[i];
                    v = v * 16 + (u32)((h >= '0' && h <= '9') ? h - '0' : (h >= 'a' && h <= 'f') ? h - 'a' + 10 : (h >= 'A' && h <= 'F') ? h - 'A' + 10 : 0);
                }

                c = (v >= 32 && v < 127) ? (char)v : '?';
            }
            else { c = e; }

            if (n < max - 1) { out[n++] = c; }

            continue;
        }

        if (n < max - 1) { out[n++] = c; }

        i++;
    }

    out[n] = 0;

    return (t[i] == '"') ? i + 1 : i;
}

int agent_parse_json(const char* text, agent_call* out)
{
    out->action = AGENT_ACT_NONE;
    out->arg1[0] = 0;
    out->arg2[0] = 0;
    out->say[0] = 0;
    out->source = AGENT_SRC_LLM;

    char action_name[48];
    int have_action = 0, have1 = 0, have2 = 0, have_say = 0;
    action_name[0] = 0;

    u32 i = 0;

    while (text[i] != 0 && text[i] != '{') { i++; }

    if (text[i] == 0) { return 0; }

    while (text[i] != 0)
    {
        if (text[i] != '"') { i++; continue; }

        char key[40];
        u32 after = json_string(text, i, key, sizeof(key));

        u32 j = after;
        while (text[j] == ' ' || text[j] == '\n' || text[j] == '\t' || text[j] == '\r') { j++; }

        if (text[j] != ':') { i = after; continue; }      /* simple valeur, pas une clef */

        j++;
        while (text[j] == ' ' || text[j] == '\n' || text[j] == '\t' || text[j] == '\r') { j++; }

        for (u32 k = 0; key[k] != 0; k++) { key[k] = lower_ascii(key[k]); }

        char value[320];
        value[0] = 0;
        int got = 0;

        if (text[j] == '"')
        {
            i = json_string(text, j, value, sizeof(value));
            got = 1;
        }
        else if ((text[j] >= '0' && text[j] <= '9') || text[j] == '-')
        {
            u32 n = 0;

            while (text[j] != 0 && text[j] != ',' && text[j] != '}' && text[j] != ' ' && text[j] != '\n' && n < sizeof(value) - 1)
            {
                value[n++] = text[j++];
            }

            value[n] = 0;
            i = j;
            got = 1;
        }
        else
        {
            i = j;   /* objet / tableau / litteral : on continue a l'interieur */
            continue;
        }

        if (!got) { continue; }

        if (!have_action && key_in(key, KEYS_ACTION)) { a_copy(action_name, sizeof(action_name), value); have_action = 1; }
        else if (!have1 && key_in(key, KEYS_ARG1))    { a_copy(out->arg1, sizeof(out->arg1), value); have1 = 1; }
        else if (!have2 && key_in(key, KEYS_ARG2))    { a_copy(out->arg2, sizeof(out->arg2), value); have2 = 1; }
        else if (!have_say && key_in(key, KEYS_SAY))  { a_copy(out->say, sizeof(out->say), value); have_say = 1; }
    }

    if (!have_action) { return 0; }

    for (u32 k = 0; action_name[k] != 0; k++) { action_name[k] = lower_ascii(action_name[k]); }

    if (a_eq(action_name, "reply") || a_eq(action_name, "say") || a_eq(action_name, "answer"))
    {
        out->action = AGENT_ACT_REPLY;
        return out->say[0] != 0;
    }

    if (a_eq(action_name, "undo"))
    {
        out->action = AGENT_ACT_UNDO;
        return 1;
    }

    out->action = agent_find(action_name);

    return out->action >= 0;
}

/* ------------------------------------------------------------
   Consignes et schema pour le modele
   ------------------------------------------------------------ */

void agent_build_system_prompt(char* out, u32 max)
{
    out[0] = 0;

    a_add(out, max,
          "Tu es Mikea, l'assistant de MikeaOS. Tu agis en choisissant UNE seule action. "
          "Reponds uniquement par un objet JSON : "
          "{\"action\":\"...\",\"arg1\":\"...\",\"arg2\":\"...\",\"say\":\"...\"}.\n"
          "Actions possibles :\n");

    for (u32 i = 0; i < g_count; i++)
    {
        a_add(out, max, "- ");
        a_add(out, max, g_reg[i].name);
        a_add(out, max, " : ");
        a_add(out, max, g_reg[i].help);
        a_add(out, max, "\n");
    }

    a_add(out, max,
          "- reply : aucune action ne convient, reponds en une ou deux phrases courtes dans say "
          "(ASCII, sans accents).\n"
          "Regles : arg1 et arg2 inutilises = chaine vide. N'invente jamais d'action. "
          "say = courte confirmation.\nExemples :\n");

    if (agent_find("open_app") >= 0)
    {
        a_add(out, max, "ouvre la calculatrice -> {\"action\":\"open_app\",\"arg1\":\"calculatrice\",\"arg2\":\"\",\"say\":\"J'ouvre la calculatrice.\"}\n");
    }

    if (agent_find("set_theme") >= 0)
    {
        a_add(out, max, "passe en mode sombre -> {\"action\":\"set_theme\",\"arg1\":\"sombre\",\"arg2\":\"\",\"say\":\"Mode sombre active.\"}\n");
    }

    a_add(out, max, "quelle est la capitale de la France -> {\"action\":\"reply\",\"arg1\":\"\",\"arg2\":\"\",\"say\":\"Paris.\"}\n");
}

void agent_build_schema(char* out, u32 max)
{
    out[0] = 0;

    a_add(out, max, "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[");

    for (u32 i = 0; i < g_count; i++)
    {
        a_add(out, max, "\"");
        a_add(out, max, g_reg[i].name);
        a_add(out, max, "\",");
    }

    a_add(out, max, "\"reply\"]},\"arg1\":{\"type\":\"string\"},\"arg2\":{\"type\":\"string\"},"
                    "\"say\":{\"type\":\"string\"}},\"required\":[\"action\",\"arg1\",\"arg2\",\"say\"]}");
}

/* ------------------------------------------------------------
   Planificateur hors ligne par regles (francais / anglais)
   ------------------------------------------------------------ */

#define MAXTOK 24
#define TOKLEN 64

typedef struct
{
    char raw[MAXTOK][TOKLEN];
    char low[MAXTOK][TOKLEN];
    u32 start[MAXTOK];     /* position du mot dans le texte d'origine */
    u32 n;
} tokens;

/* Replie un octet UTF-8 de la plage C3 80..BF vers sa lettre ASCII de base ('\0' = inconnu). */

static char fold_latin1(unsigned char c)
{
    if (c >= 0x80 && c <= 0x9F) { c = (unsigned char)(c + 0x20); }   /* majuscules accentuees */

    if (c >= 0xA0 && c <= 0xA5) { return 'a'; }
    if (c == 0xA7) { return 'c'; }
    if (c >= 0xA8 && c <= 0xAB) { return 'e'; }
    if (c >= 0xAC && c <= 0xAF) { return 'i'; }
    if (c == 0xB1) { return 'n'; }
    if (c >= 0xB2 && c <= 0xB6) { return 'o'; }
    if (c >= 0xB9 && c <= 0xBC) { return 'u'; }
    if (c == 0xBD || c == 0xBF) { return 'y'; }

    return 0;
}

static int is_sep(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\'';
}

static int is_edge_punct(char c)
{
    return c == '.' || c == ',' || c == ';' || c == ':' || c == '!' || c == '?' || c == '"'
           || c == '(' || c == ')' || c == '[' || c == ']';
}

static void tokenize(const char* text, tokens* t)
{
    t->n = 0;

    u32 i = 0;

    while (text[i] != 0 && t->n < MAXTOK)
    {
        while (text[i] != 0 && is_sep((unsigned char)text[i])) { i++; }

        /* Apostrophe typographique (E2 80 99) et guillemets francais (C2 AB / C2 BB) : separateurs. */

        if ((unsigned char)text[i] == 0xE2 && (unsigned char)text[i + 1] == 0x80 && (unsigned char)text[i + 2] == 0x99) { i += 3; continue; }
        if ((unsigned char)text[i] == 0xC2 && ((unsigned char)text[i + 1] == 0xAB || (unsigned char)text[i + 1] == 0xBB)) { i += 2; continue; }

        if (text[i] == 0) { break; }

        u32 s = i;
        char raw[TOKLEN];
        u32 r = 0;

        while (text[i] != 0 && !is_sep((unsigned char)text[i]))
        {
            if ((unsigned char)text[i] == 0xE2 && (unsigned char)text[i + 1] == 0x80 && (unsigned char)text[i + 2] == 0x99) { break; }
            if ((unsigned char)text[i] == 0xC2 && ((unsigned char)text[i + 1] == 0xAB || (unsigned char)text[i + 1] == 0xBB)) { break; }

            if (r < TOKLEN - 1) { raw[r++] = text[i]; }

            i++;
        }

        raw[r] = 0;

        /* Ponctuation de bord retiree ("notes.txt." -> "notes.txt"). */

        while (r > 0 && is_edge_punct(raw[r - 1])) { raw[--r] = 0; }

        u32 b = 0;
        while (raw[b] != 0 && is_edge_punct(raw[b])) { b++; }

        if (raw[b] == 0) { continue; }

        a_copy(t->raw[t->n], TOKLEN, raw + b);

        /* Version repliee : minuscules ASCII, accents supprimes. */

        u32 l = 0;

        for (u32 k = b; raw[k] != 0 && l < TOKLEN - 1; k++)
        {
            unsigned char c = (unsigned char)raw[k];

            if (c == 0xC3 && raw[k + 1] != 0)
            {
                char f = fold_latin1((unsigned char)raw[k + 1]);
                k++;

                if (f != 0) { t->low[t->n][l++] = f; }

                continue;
            }

            t->low[t->n][l++] = (c < 0x80) ? lower_ascii((char)c) : '?';
        }

        t->low[t->n][l] = 0;
        t->start[t->n] = s + b;

        u32 whole = t->n;
        t->n++;

        /*
            Mot compose ("sais-tu", "deconnecte-moi", "reste-t-il") : on ajoute ses
            parties APRES le mot entier, pour que les regles les reconnaissent. Pas
            pour un nom de fichier (contient un point) : "mes-notes.txt" reste entier.
            Le mot entier passe d'abord, donc il reste le premier candidat comme nom.
        */

        int has_dot = 0;
        int hyphens = 0;

        for (u32 k = 0; t->low[whole][k] != 0; k++)
        {
            if (t->low[whole][k] == '.') { has_dot = 1; }
            if (t->low[whole][k] == '-') { hyphens++; }
        }

        if (!has_dot && hyphens > 0)
        {
            /*
                Le texte brut et le texte replie n'ont pas les memes longueurs
                (un "e accent" fait 2 octets, 1 une fois replie) : on decoupe
                chacun separatement, segment par segment.
            */

            for (int seg = 0; seg <= hyphens && t->n < MAXTOK; seg++)
            {
                u32 r_start = 0, l_start = 0;
                int found = 0;

                for (u32 k = 0; t->raw[whole][k] != 0 && found < seg; k++)
                {
                    if (t->raw[whole][k] == '-') { found++; r_start = k + 1; }
                }

                found = 0;

                for (u32 k = 0; t->low[whole][k] != 0 && found < seg; k++)
                {
                    if (t->low[whole][k] == '-') { found++; l_start = k + 1; }
                }

                u32 rl = 0, ll = 0;

                while (t->raw[whole][r_start + rl] != 0 && t->raw[whole][r_start + rl] != '-' && rl < TOKLEN - 1)
                {
                    t->raw[t->n][rl] = t->raw[whole][r_start + rl];
                    rl++;
                }

                while (t->low[whole][l_start + ll] != 0 && t->low[whole][l_start + ll] != '-' && ll < TOKLEN - 1)
                {
                    t->low[t->n][ll] = t->low[whole][l_start + ll];
                    ll++;
                }

                t->raw[t->n][rl] = 0;
                t->low[t->n][ll] = 0;

                if (ll > 0)
                {
                    t->start[t->n] = t->start[whole] + r_start;
                    t->n++;
                }
            }
        }
    }
}

static int tok_idx(const tokens* t, const char* word)
{
    for (u32 i = 0; i < t->n; i++)
    {
        if (a_eq(t->low[i], word)) { return (int)i; }
    }

    return -1;
}

static int tok_any(const tokens* t, const char* const* words)
{
    for (u32 w = 0; words[w] != (const char*)0; w++)
    {
        int i = tok_idx(t, words[w]);

        if (i >= 0) { return i; }
    }

    return -1;
}

/* Premier token (dans l'ORDRE DU TEXTE) qui figure dans la liste. */

static int tok_first_any(const tokens* t, const char* const* words)
{
    for (u32 i = 0; i < t->n; i++)
    {
        for (u32 w = 0; words[w] != (const char*)0; w++)
        {
            if (a_eq(t->low[i], words[w])) { return (int)i; }
        }
    }

    return -1;
}

/* Mots qui ne sont jamais un nom de fichier. */

static const char* const W_FILLER[] = { "le", "la", "les", "un", "une", "des", "de", "du", "mon", "ma", "mes",
                                        "fichier", "file", "note", "document", "nomme", "appele", "intitule",
                                        "named", "called", "the", "a", "an", "my", "dossier", "folder", "s'il",
                                        "te", "plait", "stp", "svp", "please", (const char*)0 };

static int is_filler(const char* low)
{
    for (u32 i = 0; W_FILLER[i] != (const char*)0; i++)
    {
        if (a_eq(low, W_FILLER[i])) { return 1; }
    }

    return 0;
}

/* Premier mot apres "from" qui peut etre un nom de fichier valide. */

static int find_name_after(const tokens* t, int from, char* out, u32 max)
{
    for (u32 i = (u32)(from + 1); i < t->n; i++)
    {
        if (is_filler(t->low[i])) { continue; }

        if (agent_valid_filename(t->raw[i]))
        {
            a_copy(out, max, t->raw[i]);
            return 1;
        }

        return 0;
    }

    return 0;
}

static int set_call(agent_call* c, const char* action, const char* a1, const char* a2)
{
    int idx = agent_find(action);

    if (idx < 0) { return 0; }

    c->action = idx;
    a_copy(c->arg1, sizeof(c->arg1), a1);
    a_copy(c->arg2, sizeof(c->arg2), a2);
    c->say[0] = 0;
    c->source = AGENT_SRC_RULES;

    return 1;
}

static const char* const W_OPEN[]   = { "ouvre", "ouvrir", "lance", "lancer", "demarre", "demarrer", "open", "launch", "start", "va", "aller", (const char*)0 };
static const char* const W_CREATE[] = { "cree", "creer", "creation", "nouveau", "nouvelle", "create", "new", "make", "fais", "genere", "ecris", "ecrire", "write", (const char*)0 };
static const char* const W_DELETE[] = { "supprime", "supprimer", "efface", "effacer", "jette", "jeter", "delete", "remove", "trash", (const char*)0 };
static const char* const W_RESTORE[] = { "restaure", "restaurer", "recupere", "recuperer", "restore", (const char*)0 };
static const char* const W_READ[]   = { "lis", "lire", "read", "cat", "affiche", "montre", "show", "contenu", (const char*)0 };
static const char* const W_LISTV[]  = { "liste", "lister", "list", "ls", "dir", "affiche", "montre", "show", (const char*)0 };
static const char* const W_CONTENT[] = { "contenant", "contenu", "avec", "texte", "content", "with", "disant", (const char*)0 };

/* Texte brut a partir du mot "idx+1" jusqu'a la fin, sans guillemets de bord. */

static void text_after(const char* text, const tokens* t, int idx, char* out, u32 max)
{
    out[0] = 0;

    if (idx + 1 >= (int)t->n) { return; }

    a_copy(out, max, text + t->start[idx + 1]);

    u32 n = a_len(out);

    while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '"' || out[n - 1] == '\'')) { out[--n] = 0; }

    if (out[0] == '"' || out[0] == '\'')
    {
        for (u32 i = 0; out[i] != 0; i++) { out[i] = out[i + 1]; }
    }
}

/* Alias d'applications -> nom canonique. */

typedef struct { const char* alias; const char* app; } app_alias;

static const app_alias APP_ALIASES[] = {
    { "fichiers", "fichiers" }, { "explorateur", "fichiers" }, { "files", "fichiers" }, { "dossiers", "fichiers" },
    { "calculatrice", "calculatrice" }, { "calc", "calculatrice" }, { "calculator", "calculatrice" },
    { "parametres", "parametres" }, { "reglages", "parametres" }, { "settings", "parametres" }, { "preferences", "parametres" },
    { "terminal", "terminal" }, { "console", "terminal" }, { "shell", "terminal" },
    { "corbeille", "corbeille" }, { "trash", "corbeille" },
    { "valiha", "valiha" }, { "musique", "valiha" }, { "music", "valiha" },
    { "archad", "archad" }, { "archive", "archad" }, { "archives", "archad" },
    { "compte", "compte" }, { "account", "compte" }, { "session", "compte" },
};

#define APP_ALIAS_COUNT ((u32)(sizeof(APP_ALIASES) / sizeof(APP_ALIASES[0])))

static const char* find_app(const tokens* t)
{
    for (u32 i = 0; i < t->n; i++)
    {
        for (u32 a = 0; a < APP_ALIAS_COUNT; a++)
        {
            if (a_eq(t->low[i], APP_ALIASES[a].alias)) { return APP_ALIASES[a].app; }
        }
    }

    return (const char*)0;
}

const char* agent_resolve_app(const char* name)
{
    tokens t;

    tokenize(name, &t);

    return find_app(&t);
}

int agent_plan_rules(const char* text, agent_call* out)
{
    tokens t;
    char name[64];
    char content[128];

    out->action = AGENT_ACT_NONE;
    out->arg1[0] = 0;
    out->arg2[0] = 0;
    out->say[0] = 0;
    out->source = AGENT_SRC_RULES;

    tokenize(text, &t);

    if (t.n == 0) { return 0; }

    /* --- aide --- */

    if (tok_idx(&t, "aide") >= 0 || tok_idx(&t, "help") >= 0
        || (tok_idx(&t, "que") >= 0 && (tok_idx(&t, "sais") >= 0 || tok_idx(&t, "peux") >= 0))
        || (tok_idx(&t, "what") >= 0 && tok_idx(&t, "can") >= 0))
    {
        out->action = AGENT_ACT_REPLY;
        a_copy(out->say,sizeof(out->say),
               "Essayez : « ouvre la calculatrice », « mode sombre », « coupe le son », « fond d’écran 3 », "
               "« crée un fichier notes.txt », « liste les fichiers », « infos système ». "
               "Dites « annule » pour défaire la dernière action.");
        return 1;
    }

    /* --- annuler --- */

    {
        static const char* const W[] = { "annule", "annuler", "undo", "defais", "defaire", (const char*)0 };

        if (tok_any(&t, W) >= 0)
        {
            out->action = AGENT_ACT_UNDO;
            return 1;
        }
    }

    /* --- fichiers : creer / supprimer / restaurer / lire (nom explicite) --- */

    {
        int cr = tok_any(&t, W_CREATE);
        int has_file_word = (tok_idx(&t, "fichier") >= 0 || tok_idx(&t, "file") >= 0 || tok_idx(&t, "note") >= 0 || tok_idx(&t, "document") >= 0);

        if (cr >= 0 && has_file_word)
        {
            if (!find_name_after(&t, cr, name, sizeof(name))) { a_copy(name, sizeof(name), "note.txt"); }

            content[0] = 0;

            int c = tok_first_any(&t, W_CONTENT);

            if (c >= 0)
            {
                /* "avec le texte bonjour" : on saute "le" et "texte", le contenu commence a "bonjour". */

                static const char* const SKIP[] = { "avec", "contenant", "contenu", "texte", "content", "with", "disant",
                                                    "le", "la", "les", "un", "une", "du", "de", "des", "the", "a", "an",
                                                    (const char*)0 };

                while (c + 1 < (int)t.n)
                {
                    int skip = 0;

                    for (u32 w = 0; SKIP[w] != (const char*)0; w++)
                    {
                        if (a_eq(t.low[c + 1], SKIP[w])) { skip = 1; }
                    }

                    if (!skip) { break; }

                    c++;
                }

                text_after(text, &t, c, content, sizeof(content));
            }

            return set_call(out, "create_file", name, content);
        }

        int dl = tok_any(&t, W_DELETE);

        if (dl >= 0 && find_name_after(&t, dl, name, sizeof(name)))
        {
            return set_call(out, "delete_file", name, "");
        }

        int rs = tok_any(&t, W_RESTORE);

        if (rs >= 0 && find_name_after(&t, rs, name, sizeof(name)))
        {
            return set_call(out, "restore_file", name, "");
        }

        /* Lire : un nom au singulier apres "fichier", ou un mot contenant un point. */

        int rd = tok_any(&t, W_READ);

        if (rd >= 0 && (tok_idx(&t, "fichier") >= 0 || tok_idx(&t, "file") >= 0))
        {
            if (find_name_after(&t, rd, name, sizeof(name))) { return set_call(out, "read_file", name, ""); }
        }

        if (rd >= 0)
        {
            for (u32 i = (u32)rd + 1; i < t.n; i++)
            {
                u32 k = 0;
                int dot = 0;

                while (t.raw[i][k] != 0) { if (t.raw[i][k] == '.') { dot = 1; } k++; }

                if (dot && agent_valid_filename(t.raw[i])) { return set_call(out, "read_file", t.raw[i], ""); }
            }
        }
    }

    /* --- theme --- */

    {
        static const char* const TRIG[] = { "mode", "theme", "apparence", "dark", "light", "nuit", (const char*)0 };
        int trig = tok_any(&t, TRIG);

        if (trig >= 0)
        {
            if (tok_idx(&t, "sombre") >= 0 || tok_idx(&t, "dark") >= 0 || tok_idx(&t, "nuit") >= 0) { return set_call(out, "set_theme", "sombre", ""); }
            if (tok_idx(&t, "clair") >= 0 || tok_idx(&t, "light") >= 0 || tok_idx(&t, "jour") >= 0) { return set_call(out, "set_theme", "clair", ""); }
            if (tok_idx(&t, "theme") >= 0) { return set_call(out, "set_theme", "basculer", ""); }
        }
    }

    /* --- son --- */

    {
        static const char* const SON[] = { "son", "sons", "sound", "audio", "volume", (const char*)0 };
        static const char* const OFF[] = { "coupe", "couper", "mute", "desactive", "desactiver", "eteins", "eteindre", "silence", "off", "stop", "sans", (const char*)0 };
        static const char* const ON[]  = { "active", "activer", "remets", "remettre", "rallume", "allume", "unmute", "on", (const char*)0 };

        int has_son = tok_any(&t, SON) >= 0;
        int off = tok_any(&t, OFF);
        int on = tok_any(&t, ON);

        if ((has_son || tok_idx(&t, "mute") >= 0 || tok_idx(&t, "silence") >= 0) && (off >= 0 || on >= 0))
        {
            /* "desactive" contient une negation : off l'emporte si les deux mots sont presents. */
            return set_call(out, "set_sound", (off >= 0) ? "off" : "on", "");
        }
    }

    /* --- fond d'ecran --- */

    {
        static const char* const FOND[] = { "fond", "wallpaper", "arriere", "background", (const char*)0 };

        if (tok_any(&t, FOND) >= 0)
        {
            for (u32 i = 0; i < t.n; i++)
            {
                if (t.low[i][0] >= '1' && t.low[i][0] <= '9' && t.low[i][1] == 0)
                {
                    return set_call(out, "set_wallpaper", t.low[i], "");
                }
            }

            return set_call(out, "set_wallpaper", "suivant", "");
        }
    }

    /* --- infos systeme --- */

    {
        static const char* const MEM[] = { "memoire", "ram", (const char*)0 };
        static const char* const DSK[] = { "disque", "stockage", "espace", (const char*)0 };
        static const char* const TIME[] = { "heure", "date", "horloge", (const char*)0 };

        if (tok_any(&t, MEM) >= 0) { return set_call(out, "system_info", "memoire", ""); }
        if (tok_any(&t, DSK) >= 0) { return set_call(out, "system_info", "disque", ""); }
        if (tok_any(&t, TIME) >= 0) { return set_call(out, "system_info", "heure", ""); }

        if ((tok_idx(&t, "infos") >= 0 || tok_idx(&t, "info") >= 0 || tok_idx(&t, "etat") >= 0 || tok_idx(&t, "informations") >= 0)
            && (tok_idx(&t, "systeme") >= 0 || tok_idx(&t, "system") >= 0))
        {
            return set_call(out, "system_info", "tout", "");
        }
    }

    /* --- lister les fichiers (pluriel / dossier) --- */

    {
        int lv = tok_any(&t, W_LISTV);
        int plural = tok_idx(&t, "fichiers") >= 0 || tok_idx(&t, "files") >= 0 || tok_idx(&t, "dossier") >= 0 || tok_idx(&t, "repertoire") >= 0;
        int is_app_open = tok_any(&t, W_OPEN) >= 0;

        if (lv >= 0 && plural && !is_app_open)
        {
            name[0] = 0;

            int d = tok_idx(&t, "dossier");

            if (d >= 0) { find_name_after(&t, d, name, sizeof(name)); }

            return set_call(out, "list_files", name, "");
        }
    }

    /* --- deconnexion --- */

    if (tok_idx(&t, "deconnecte") >= 0 || tok_idx(&t, "deconnexion") >= 0 || tok_idx(&t, "logout") >= 0
        || (tok_idx(&t, "ferme") >= 0 && tok_idx(&t, "session") >= 0))
    {
        return set_call(out, "logout", "", "");
    }

    /* --- ouvrir une application (ou simplement son nom) --- */

    {
        const char* app = find_app(&t);

        if (app != (const char*)0 && (tok_any(&t, W_OPEN) >= 0 || t.n == 1))
        {
            return set_call(out, "open_app", app, "");
        }
    }

    return 0;
}
