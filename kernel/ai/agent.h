#ifndef MIKEA_AGENT_H
#define MIKEA_AGENT_H

#include "../../include/types.h"

/*
    ============================================================
    Mikea OS - Agent IA (cerveau + garde-fous)
    ============================================================

    Transforme une demande en langage naturel ("ouvre la calculatrice",
    "passe en mode sombre", "cree un fichier notes.txt") en UNE action
    precise du systeme, avec des garde-fous :

      1. LISTE FERMEE : l'agent ne peut appeler que les actions
         enregistrees (agent_set_registry). Un nom inconnu, invente par
         un modele, est refuse. Aucun acces a un shell, jamais.
      2. ARGUMENTS VALIDES : chaque action valide ses arguments (noms de
         fichiers sans chemin ni "..", valeurs d'une liste connue...).
      3. PERMISSIONS A 4 NIVEAUX (agent_perm) : lecture et reglages
         reversibles s'executent seuls ; l'ecriture demande une
         confirmation ; la suppression demande une confirmation forte.
      4. ANNULATION : les actions reversibles empilent un "undo".
      5. JOURNAL : les 16 dernieres actions sont gardees.

    Deux planificateurs, meme sortie (agent_call) et memes garde-fous :
      - agent_plan_rules() : regles francais/anglais, 100 % hors ligne,
        instantane, deterministe.
      - un modele local (Ollama, voir kernel/ai/agent_actions.c) dont la
        sortie est CONTRAINTE par un schema JSON (agent_build_schema) puis
        relue par agent_parse_json() : un modele ne peut rien faire de plus
        que ce que le registre autorise.

    Ce fichier ne depend d'aucun module du noyau (testable sur l'hote,
    voir tools/tests/agent_host_test.c). Les actions reelles sont
    branchees dans agent_actions.c.
    ============================================================
*/

typedef enum
{
    AGENT_PERM_READ = 0,         /* lit / affiche : s'execute seule */
    AGENT_PERM_REVERSIBLE = 1,   /* change un reglage ou ouvre une app : seule, avec annulation */
    AGENT_PERM_WRITE = 2,        /* cree / modifie des donnees : confirmation */
    AGENT_PERM_DESTRUCTIVE = 3   /* supprime, deconnecte : confirmation forte */
} agent_perm;

typedef struct
{
    char data[2][64];            /* ce dont l'action a besoin pour se defaire */
} agent_undo_info;

typedef struct
{
    const char* name;            /* identifiant, ex. "open_app" (ASCII, sans espace) */
    const char* help;            /* description pour le modele (francais, ASCII) */
    agent_perm perm;
    const char* confirm;         /* texte de confirmation, %1 / %2 = arguments (peut etre NULL) */

    /* 1 = arguments acceptables. NULL = accepte tout (apres le nettoyage generique). */
    int (*validate)(const char* a1, const char* a2);

    /* Execute. Ecrit un message pour l'utilisateur dans out. Renvoie 1 = succes, 0 = echec. */
    int (*run)(const char* a1, const char* a2, char* out, u32 outmax, agent_undo_info* undo);

    /* NULL = non annulable. Appelee avec l'info remplie par run(). */
    int (*undo)(const agent_undo_info* info, char* out, u32 outmax);
} agent_action;

/* Index speciaux dans agent_call.action. */

#define AGENT_ACT_NONE   (-1)
#define AGENT_ACT_UNDO   (-2)    /* "annule" : defait la derniere action */
#define AGENT_ACT_REPLY  (-3)    /* simple reponse texte (champ say) */

typedef enum { AGENT_SRC_RULES = 0, AGENT_SRC_LLM = 1 } agent_source;

typedef struct
{
    int action;                  /* index dans le registre, ou AGENT_ACT_* */
    char arg1[64];
    char arg2[128];
    char say[320];               /* reponse (action "reply") */
    agent_source source;
} agent_call;

typedef enum
{
    AGENT_DECISION_RUN = 0,             /* peut s'executer tout de suite */
    AGENT_DECISION_CONFIRM = 1,         /* demander "Valider ?" */
    AGENT_DECISION_CONFIRM_STRONG = 2,  /* confirmation forte (action destructive) */
    AGENT_DECISION_REJECT = 3           /* refusee : voir "reason" */
} agent_decision;

/* ---- Registre ----------------------------------------------------- */

void agent_set_registry(const agent_action* table, u32 count);
u32 agent_action_count(void);
const agent_action* agent_action_at(u32 index);
int agent_find(const char* name);                 /* index ou AGENT_ACT_NONE */

/* ---- Planification ------------------------------------------------ */

/* Regles hors ligne. 1 = une action (ou reponse) a ete deduite, 0 = rien compris. */
int agent_plan_rules(const char* text, agent_call* out);

/*
    Lit la sortie d'un modele : cherche un objet JSON {"action":..,"arg1":..,"arg2":..,"say":..}
    (tolere texte autour, clefs imbriquees, nombres). 1 = action valide du registre,
    "reply" ou "undo" ; 0 sinon.
*/
int agent_parse_json(const char* text, agent_call* out);

/* Consignes + schema JSON a envoyer au modele, construits depuis le registre. */
void agent_build_system_prompt(char* out, u32 max);
void agent_build_schema(char* out, u32 max);

/* ---- Garde-fous et execution -------------------------------------- */

agent_decision agent_decide(const agent_call* call, char* reason, u32 reason_max);

/* Texte "Creer le fichier 'x' ?" pour la confirmation. */
void agent_describe(const agent_call* call, char* out, u32 max);

/*
    SEUL point d'execution. Re-verifie tout (action connue, arguments valides)
    puis applique la regle de permission : une action AGENT_DECISION_CONFIRM ou
    _CONFIRM_STRONG n'est executee que si "confirmed" vaut 1 (l'utilisateur a
    clique "Valider") ; sinon elle est refusee. Journalise et empile l'annulation.
    Renvoie 1 = succes, 0 = refus ou echec (message dans "out").
*/
int agent_execute(const agent_call* call, int confirmed, char* out, u32 outmax);

int agent_undo_available(void);
int agent_undo_last(char* out, u32 outmax);

/* ---- Journal ------------------------------------------------------ */

typedef struct
{
    u32 seq;
    int action;
    char arg1[24];
    int ok;
    int undone;
    agent_source source;
} agent_log_entry;

u32 agent_log_count(void);
const agent_log_entry* agent_log_at(u32 index);   /* 0 = la plus recente */

/* ---- Utilitaires partages ----------------------------------------- */

/*
    Nom canonique d'une application a partir d'un nom/alias ("reglages" ->
    "parametres", "calc" -> "calculatrice"...), ou NULL. Insensible a la casse
    et aux accents. Table partagee avec le planificateur par regles.
*/
const char* agent_resolve_app(const char* name);

/* Nom de fichier simple : [A-Za-z0-9._-], 1 a 31 caracteres, pas de point initial, pas de "..". */
int agent_valid_filename(const char* name);

#endif
