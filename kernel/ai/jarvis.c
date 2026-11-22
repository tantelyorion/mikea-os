#include "jarvis.h"
#include "../drivers/serial/serial.h"
#include "../../net/net.h"
#include "../../libc/string.h"

void console_write(const char* text);
void execute_command(char* command);

#define JARVIS_MAX_INPUT 160

/* ------------------------------------------------------------
   Utilitaires texte (ASCII)
   ------------------------------------------------------------ */

static void to_lower_copy(const char* src, char* dst, u32 max)
{
    u32 i = 0;

    while (src[i] != 0 && i < max - 1)
    {
        char c = src[i];
        if (c >= 'A' && c <= 'Z')
        {
            c = (char)(c + 32);
        }
        dst[i] = c;
        i++;
    }
    dst[i] = 0;
}

static int contains(const char* haystack, const char* needle)
{
    u32 nlen = mk_strlen(needle);

    if (nlen == 0)
    {
        return 1;
    }

    for (u32 i = 0; haystack[i] != 0; i++)
    {
        if (mk_strncmp(haystack + i, needle, nlen) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static int contains_any(const char* haystack, const char* const* words)
{
    for (int i = 0; words[i] != (const char*)0; i++)
    {
        if (contains(haystack, words[i]))
        {
            return 1;
        }
    }
    return 0;
}

/* Cherche un token "a.b.c.d" valide dans la phrase. */
static int find_ip(const char* text, u8 out[4])
{
    char token[24];
    u32 t = 0;

    for (u32 i = 0;; i++)
    {
        char c = text[i];

        if ((c >= '0' && c <= '9') || c == '.')
        {
            if (t < sizeof(token) - 1)
            {
                token[t++] = c;
            }
        }
        else
        {
            if (t > 0)
            {
                token[t] = 0;
                if (net_parse_ip(token, out))
                {
                    return 1;
                }
            }
            t = 0;

            if (c == 0)
            {
                break;
            }
        }
    }
    return 0;
}

static void run(const char* command)
{
    char buffer[JARVIS_MAX_INPUT];
    mk_strcpy(buffer, command, sizeof(buffer));
    execute_command(buffer);
}

/* ------------------------------------------------------------
   Pont vers le modele local (port serie)
   ------------------------------------------------------------ */

static int bridge_state = 0; /* 0 = inconnu, 1 = connecte, -1 = absent */

static int read_line(char* out, u32 max, u32 first_timeout, u32 next_timeout)
{
    u32 n = 0;
    int c = serial_getc_timeout(first_timeout);

    while (c >= 0)
    {
        if (c == '\n')
        {
            out[n] = 0;
            return 1;
        }
        if (c != '\r' && n < max - 1)
        {
            out[n++] = (char)c;
        }
        c = serial_getc_timeout(next_timeout);
    }

    out[n] = 0;
    return n > 0;
}

static int bridge_probe()
{
    char reply[16];

    while (serial_getc_timeout(0) >= 0)
    {
        /* vider les octets residuels */
    }

    serial_write("P:\n");

    if (read_line(reply, sizeof(reply), 100, 20) && reply[0] == 'O' && reply[1] == 'K')
    {
        return 1;
    }
    return 0;
}

static int bridge_ask(const char* question, char* answer, u32 max)
{
    if (bridge_state == 0)
    {
        bridge_state = bridge_probe() ? 1 : -1;
    }

    if (bridge_state != 1)
    {
        /* Nouvelle tentative a chaque demande explicite. */
        bridge_state = bridge_probe() ? 1 : -1;

        if (bridge_state != 1)
        {
            return 0;
        }
    }

    serial_write("Q:");
    serial_write(question);
    serial_write("\n");

    char line[260];

    if (!read_line(line, sizeof(line), 12000, 300))
    {
        return 0;
    }

    const char* text = line;
    if (line[0] == 'A' && line[1] == ':')
    {
        text = line + 2;
    }

    mk_strcpy(answer, text, max);
    return 1;
}

/* ------------------------------------------------------------
   Moteur d'intentions
   ------------------------------------------------------------ */

static const char* const W_HELLO[]   = { "bonjour", "salut", "hello", "coucou", "bonsoir", (const char*)0 };
static const char* const W_WHO[]     = { "qui es", "who are", "presente", "t'appelles", (const char*)0 };
static const char* const W_NET[]     = { "reseau", "network", "internet", "connexion", "ethernet", "adresse ip", "ifconfig", "netinfo", (const char*)0 };
static const char* const W_SEC[]     = { "securite", "security", "intrusion", "alerte", "attaque", "journal", "seclog", "menace", (const char*)0 };
static const char* const W_FW[]      = { "pare-feu", "parefeu", "firewall", "protege", (const char*)0 };
static const char* const W_HW[]      = { "materiel", "hardware", "pci", "peripherique", "composant", (const char*)0 };
static const char* const W_RADIO[]   = { "wifi", "wi-fi", "wlan", "bluetooth", "gps", "localisation", "location", "position", (const char*)0 };
static const char* const W_CPU[]     = { "cpu", "processeur", (const char*)0 };
static const char* const W_MEM[]     = { "memoire", "memory", " ram", (const char*)0 };
static const char* const W_ARP[]     = { "arp", "voisin", "neighbor", (const char*)0 };
static const char* const W_HELP[]    = { "aide", "help", "que sais-tu", "que peux-tu", "what can", (const char*)0 };
static const char* const W_CRYPTO[]  = { "crypto", "chiffrement", "selftest", "teste la securite", (const char*)0 };
static const char* const W_CLEAR[]   = { "efface", "nettoie l'ecran", "clear", (const char*)0 };

static void say(const char* text)
{
    console_write("JARVIS> ");
    console_write(text);
    console_write("\n");
}

static void jarvis_help()
{
    say("Je comprends des phrases simples, par exemple :");
    console_write("   - \"jarvis etat du reseau\"        - \"jarvis ping 10.0.2.2\"\n");
    console_write("   - \"jarvis journal de securite\"   - \"jarvis pare-feu\"\n");
    console_write("   - \"jarvis materiel\"              - \"jarvis wifi / bluetooth / gps\"\n");
    console_write("   - \"jarvis cpu\" / \"jarvis memoire\"\n");
    console_write("   - \"jarvis ask <question libre>\" : interroge le modele IA local\n");
    console_write("     (Ollama sur votre PC : 'ollama serve') | jarvis model <nom>\n");
}

/* Renvoie 1 si une intention a ete reconnue et traitee. */
static int handle_intent(const char* lower)
{
    u8 ip[4];

    if (contains(lower, "ping") || contains(lower, "teste la connexion vers") || contains(lower, "joins"))
    {
        if (find_ip(lower, ip))
        {
            say("Je lance le test de connectivite.");
            net_ping(ip);
            return 1;
        }
        say("Donnez-moi une adresse, par exemple : jarvis ping 10.0.2.2");
        return 1;
    }

    if (contains_any(lower, W_FW))
    {
        say("Voici l'etat du pare-feu.");
        run("firewall");
        return 1;
    }

    if (contains_any(lower, W_SEC))
    {
        say("Je consulte le journal de securite.");
        run("seclog");
        return 1;
    }

    if (contains_any(lower, W_ARP))
    {
        say("Voici la table des voisins ARP.");
        run("arp");
        return 1;
    }

    if (contains_any(lower, W_RADIO))
    {
        say("Etat des radios et de la localisation.");
        run("radio");
        return 1;
    }

    if (contains_any(lower, W_NET))
    {
        say("Voici l'etat du reseau.");
        run("netinfo");
        return 1;
    }

    if (contains_any(lower, W_HW))
    {
        say("Inventaire du materiel detecte.");
        run("lspci");
        return 1;
    }

    if (contains_any(lower, W_CPU))
    {
        run("cpu");
        return 1;
    }

    if (contains_any(lower, W_MEM))
    {
        run("mem");
        return 1;
    }

    if (contains_any(lower, W_CRYPTO))
    {
        say("Je lance les tests cryptographiques.");
        run("selftest");
        return 1;
    }

    if (contains_any(lower, W_CLEAR))
    {
        run("clear");
        return 1;
    }

    if (contains_any(lower, W_WHO))
    {
        say("Je suis JARVIS, l'assistant local de MikeaOS. Je tourne sur cette machine,");
        console_write("        sans cloud : vos donnees ne la quittent pas.\n");
        return 1;
    }

    if (contains_any(lower, W_HELP))
    {
        jarvis_help();
        return 1;
    }

    if (contains_any(lower, W_HELLO))
    {
        say("A votre service. Que puis-je faire ?");
        return 1;
    }

    return 0;
}

/* ------------------------------------------------------------
   Modele local via Ollama, directement par le reseau (TCP)
   ------------------------------------------------------------ */

/*
    Dans QEMU ("-nic user"), l'adresse de la passerelle (10.0.2.2) est la
    machine HOTE. Ollama y ecoute par defaut sur 127.0.0.1:11434 :
    JARVIS lui parle donc en HTTP, sans pont ni internet.

    SECURITE : la destination est TOUJOURS la passerelle (l'hote local),
    jamais une adresse choisie par l'utilisateur ou par le modele ; le
    texte renvoye est affiche, jamais execute.
*/

#define OLLAMA_PORT 11434

/* 96 : un modele tire de Hugging Face s'ecrit "hf.co/Organisation/Depot:Q4_K_M" (souvent > 48). */
static char jarvis_model[96] = "llama3.2:3b";

static const char* const JARVIS_SYSTEM =
    "Tu es JARVIS, l'assistant du systeme MikeaOS. Reponds en francais, en une a trois "
    "phrases courtes, sans markdown, en ASCII sans accents.";

static void append_text(char* dst, u32* n, u32 max, const char* text)
{
    for (u32 i = 0; text[i] != 0 && *n < max - 1; i++)
    {
        dst[(*n)++] = text[i];
    }
    dst[*n] = 0;
}

static void json_escape(const char* src, char* dst, u32 max)
{
    u32 n = 0;

    for (u32 i = 0; src[i] != 0 && n + 3 < max; i++)
    {
        unsigned char c = (unsigned char)src[i];

        if (c == '"' || c == '\\')
        {
            dst[n++] = '\\';
            dst[n++] = (char)c;
        }
        else if (c == '\n')
        {
            dst[n++] = '\\';
            dst[n++] = 'n';
        }
        else if (c < 32 || c > 126)
        {
            dst[n++] = ' ';
        }
        else
        {
            dst[n++] = (char)c;
        }
    }
    dst[n] = 0;
}

/* Remplace un caractere Unicode (U+0080..U+00FF) par son equivalent ASCII. */
static char ascii_fold(u32 cp)
{
    if (cp < 128)
    {
        return (char)cp;
    }
    if ((cp >= 0xC0 && cp <= 0xC5) || (cp >= 0xE0 && cp <= 0xE5)) { return (cp < 0xE0) ? 'A' : 'a'; }
    if (cp == 0xC7) { return 'C'; }
    if (cp == 0xE7) { return 'c'; }
    if ((cp >= 0xC8 && cp <= 0xCB) || (cp >= 0xE8 && cp <= 0xEB)) { return (cp < 0xE0) ? 'E' : 'e'; }
    if ((cp >= 0xCC && cp <= 0xCF) || (cp >= 0xEC && cp <= 0xEF)) { return (cp < 0xE0) ? 'I' : 'i'; }
    if ((cp >= 0xD2 && cp <= 0xD6) || (cp >= 0xF2 && cp <= 0xF6)) { return (cp < 0xE0) ? 'O' : 'o'; }
    if ((cp >= 0xD9 && cp <= 0xDC) || (cp >= 0xF9 && cp <= 0xFC)) { return (cp < 0xE0) ? 'U' : 'u'; }
    return '?';
}

/* Extrait la valeur de "response":"..." d'une reponse JSON Ollama. 1 si trouvee. */
static int extract_response(const char* data, u32 length, char* out, u32 max)
{
    static const char key[] = "\"response\":\"";
    u32 klen = sizeof(key) - 1;
    u32 start = 0;
    int found = 0;

    for (u32 i = 0; i + klen <= length; i++)
    {
        if (mk_strncmp(data + i, key, klen) == 0)
        {
            start = i + klen;
            found = 1;
            break;
        }
    }

    if (!found)
    {
        return 0;
    }

    u32 n = 0;

    for (u32 i = start; i < length && n < max - 1; i++)
    {
        unsigned char c = (unsigned char)data[i];

        if (c == '"')
        {
            break;
        }

        if (c == '\\' && i + 1 < length)
        {
            char e = data[++i];

            if (e == 'n' || e == 't' || e == 'r') { out[n++] = ' '; }
            else if (e == 'u' && i + 4 < length)
            {
                u32 cp = 0;
                for (int k = 1; k <= 4; k++)
                {
                    char h = data[i + k];
                    cp = cp * 16 + (u32)((h >= '0' && h <= '9') ? h - '0' : (h >= 'a' && h <= 'f') ? h - 'a' + 10 : (h >= 'A' && h <= 'F') ? h - 'A' + 10 : 0);
                }
                i += 4;
                out[n++] = ascii_fold(cp);
            }
            else { out[n++] = e; }
        }
        else if (c >= 0xC0 && c < 0xE0 && i + 1 < length)
        {
            u32 cp = ((u32)(c & 0x1F) << 6) | ((unsigned char)data[i + 1] & 0x3F);
            i++;
            out[n++] = ascii_fold(cp);
        }
        else if (c >= 0x80)
        {
            /* Caracteres UTF-8 longs (emojis, symboles) : ignores. */
            while (i + 1 < length && ((unsigned char)data[i + 1] & 0xC0) == 0x80)
            {
                i++;
            }
        }
        else if (c >= 32)
        {
            out[n++] = (char)c;
        }
    }

    out[n] = 0;
    return n > 0;
}

/*
    Appel generique du modele local (Ollama, /api/generate, sans streaming).

      system   : consigne systeme (texte brut, echappe ici)
      prompt   : demande de l'utilisateur
      schema   : NULL pour une reponse libre ; sinon un SCHEMA JSON (objet) :
                 Ollama contraint alors le decodage du modele, qui ne peut
                 produire QUE du JSON conforme (sortie structuree). Utilise
                 par l'agent (kernel/ai/agent_kernel.c) avec temperature 0.
      predict  : nombre maximal de jetons generes
      temp     : temperature, en texte ("0" ou "0.3")

    Les tampons sont statiques (la pile d'un fil du noyau est petite) :
    un seul appel a la fois.
*/

static int ollama_generate(const char* system, const char* prompt, const char* schema,
                           u32 predict, const char* temp, char* answer, u32 max)
{
    if (!net_is_up())
    {
        return 0;
    }

    static char body[3800];
    static char request[4100];
    static char response[6144];
    static char escaped_system[2600];
    char escaped_prompt[400];

    json_escape(system, escaped_system, sizeof(escaped_system));
    json_escape(prompt, escaped_prompt, sizeof(escaped_prompt));

    u32 b = 0;
    body[0] = 0;

    append_text(body, &b, sizeof(body), "{\"model\":\"");
    append_text(body, &b, sizeof(body), jarvis_model);
    append_text(body, &b, sizeof(body), "\",\"system\":\"");
    append_text(body, &b, sizeof(body), escaped_system);
    append_text(body, &b, sizeof(body), "\",\"prompt\":\"");
    append_text(body, &b, sizeof(body), escaped_prompt);
    append_text(body, &b, sizeof(body), "\",\"stream\":false,");

    if (schema != (const char*)0)
    {
        append_text(body, &b, sizeof(body), "\"format\":");
        append_text(body, &b, sizeof(body), schema);
        append_text(body, &b, sizeof(body), ",");
    }

    char predict_text[12];
    {
        u32 v = predict;
        int d = 0;
        char rev[12];

        if (v == 0) { rev[d++] = '0'; }

        while (v > 0 && d < 11) { rev[d++] = (char)('0' + v % 10); v /= 10; }

        for (int k = 0; k < d; k++) { predict_text[k] = rev[d - 1 - k]; }

        predict_text[d] = 0;
    }

    append_text(body, &b, sizeof(body), "\"options\":{\"num_predict\":");
    append_text(body, &b, sizeof(body), predict_text);
    append_text(body, &b, sizeof(body), ",\"temperature\":");
    append_text(body, &b, sizeof(body), temp);
    append_text(body, &b, sizeof(body), "}}");

    char length_text[12];
    u32 value = b;
    int digits = 0;
    char reversed[12];

    if (value == 0) { reversed[digits++] = '0'; }

    while (value > 0 && digits < 11) { reversed[digits++] = (char)('0' + value % 10); value /= 10; }

    for (int i = 0; i < digits; i++) { length_text[i] = reversed[digits - 1 - i]; }

    length_text[digits] = 0;

    u32 r = 0;
    request[0] = 0;

    append_text(request, &r, sizeof(request), "POST /api/generate HTTP/1.1\r\nHost: host:11434\r\n"
                                              "Content-Type: application/json\r\nContent-Length: ");
    append_text(request, &r, sizeof(request), length_text);
    append_text(request, &r, sizeof(request), "\r\nConnection: close\r\n\r\n");
    append_text(request, &r, sizeof(request), body);

    if (tcp_connect(net_gateway, OLLAMA_PORT) != 0)
    {
        return 0;
    }

    if (tcp_write((const u8*)request, r) != 0)
    {
        tcp_close();
        return 0;
    }

    u32 total = 0;

    while (total < sizeof(response) - 1)
    {
        /* Un modele peut mettre plusieurs dizaines de secondes a repondre. */
        int got = tcp_read((u8*)response + total, (u32)(sizeof(response) - 1 - total), 6000);

        if (got <= 0)
        {
            break;
        }

        total += (u32)got;
    }

    tcp_close();
    response[total] = 0;

    return extract_response(response, total, answer, max);
}

static int ollama_ask(const char* question, char* answer, u32 max)
{
    return ollama_generate(JARVIS_SYSTEM, question, (const char*)0, 120, "0.3", answer, max);
}

/* ---- API pour l'agent ---- */

int jarvis_llm_available(void)
{
    return net_is_up();
}

int jarvis_llm_generate(const char* system, const char* prompt, const char* schema_json, char* out, u32 max)
{
    return ollama_generate(system, prompt, schema_json, 160, "0", out, max);
}

const char* jarvis_get_model(void)
{
    return jarvis_model;
}

void jarvis_set_model(const char* name)
{
    mk_strcpy(jarvis_model, name, sizeof(jarvis_model));
}

static void jarvis_ask_model(const char* question)
{
    char answer[260];

    say("Je consulte le modele IA local...");

    /* 1) Ollama sur l'hote, directement par le reseau. */
    if (ollama_ask(question, answer, sizeof(answer)))
    {
        say(answer);
        return;
    }

    /* 2) Pont serie (tools/jarvis_bridge.py). */
    if (bridge_ask(question, answer, sizeof(answer)))
    {
        say(answer);
        return;
    }

    say("Aucun modele IA local n'a repondu.");
    console_write("        Lancez Ollama sur votre PC ('ollama serve', modele ");
    console_write(jarvis_model);
    console_write("),\n        ou le pont : python3 tools/jarvis_bridge.py puis make run-ai.\n");
}

int jarvis_command(char* line)
{
    /* line commence par "jarvis" (verifie par l'appelant). */
    const char* text = line + 6;

    while (*text == ' ')
    {
        text++;
    }

    if (*text == 0)
    {
        say("A votre service. Tapez 'jarvis aide' pour voir ce que je sais faire.");
        return 1;
    }

    /* "jarvis model <nom>" : change le modele Ollama utilise. */
    if (mk_strncmp(text, "model ", 6) == 0)
    {
        mk_strcpy(jarvis_model, text + 6, sizeof(jarvis_model));
        say("Modele IA regle sur :");
        console_write("        ");
        console_write(jarvis_model);
        console_write("\n");
        return 1;
    }

    /* "jarvis ask ..." : question ouverte, directement au modele. */
    if (mk_strncmp(text, "ask ", 4) == 0)
    {
        jarvis_ask_model(text + 4);
        return 1;
    }

    char lower[JARVIS_MAX_INPUT];
    to_lower_copy(text, lower, sizeof(lower));

    if (handle_intent(lower))
    {
        return 1;
    }

    /* Aucune intention connue : on tente le modele de langage local. */
    jarvis_ask_model(text);
    return 1;
}
