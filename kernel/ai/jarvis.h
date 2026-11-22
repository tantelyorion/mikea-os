#ifndef MIKEA_JARVIS_H
#define MIKEA_JARVIS_H

#include "../../include/types.h"

/*
    JARVIS -- assistant IA local de MikeaOS.

    Deux niveaux, tous deux LOCAUX (aucune donnee ne quitte la
    machine) :

      1. Moteur d'intentions embarque (francais/anglais) : comprend
         des phrases comme "montre-moi l'etat du reseau" ou
         "ping 10.0.2.2" et execute la commande correspondante.
         Fonctionne seul, instantanement, sans dependance.

      2. Pont vers un modele de langage local (Ollama / llama.cpp)
         qui tourne sur la machine hote, via le port serie COM1
         (voir tools/jarvis_bridge.py). Utilise pour les questions
         ouvertes que le moteur d'intentions ne couvre pas.

    SECURITE : la reponse du modele est AFFICHEE, jamais executee.
    Seul le moteur d'intentions deterministe (niveau 1) peut lancer
    une commande, et uniquement parmi une liste blanche.
*/

/* Traite "jarvis ..." (ligne complete). Renvoie 1 si traite. */
int jarvis_command(char* line);

/*
    --- Pour l'agent (kernel/ai/agent_kernel.c) ---

    jarvis_llm_generate() interroge le modele local (Ollama sur l'hote) avec une
    consigne systeme et un SCHEMA JSON : la sortie du modele est contrainte par ce
    schema (decodage structure d'Ollama, temperature 0). 1 = reponse recue.
*/
int jarvis_llm_available(void);

int jarvis_llm_generate(const char* system, const char* prompt, const char* schema_json, char* out, u32 max);

const char* jarvis_get_model(void);

void jarvis_set_model(const char* name);

#endif
