#ifndef MIKEA_AGENT_KERNEL_H
#define MIKEA_AGENT_KERNEL_H

#include "agent.h"

/*
    ============================================================
    Mikea OS - Agent : liaison avec le vrai systeme
    ============================================================

    Branche les 11 actions de agent_builtin.c sur le noyau reel
    (applications, theme, son, fond d'ecran, fichiers, corbeille...)
    et choisit le "cerveau" :

        1. regles hors ligne (agent_plan_rules) : instantane, sans reseau ;
        2. sinon, le modele local (Ollama sur l'hote) avec une SORTIE
           CONTRAINTE par schema JSON, relue par agent_parse_json() ;
        3. sinon, une reponse "aucun modele joignable" -- les regles
           continuent de fonctionner.

    Dans tous les cas la decision passe par les memes garde-fous
    (agent_decide / agent_execute) : aucune difference de droits entre
    une regle et un modele. L'agent agit avec les droits de
    l'utilisateur connecte (les fonctions de fichiers verifient deja
    current_user_can()), jamais davantage.
    ============================================================
*/

/* Idempotent : installe la plateforme et le registre. */
void agent_kernel_init(void);

/*
    Comprend "text". Renvoie 1 si "out" contient quelque chose a presenter
    (action, annulation ou reponse) ; 0 si la demande est vide.
    *used_llm = 1 si le modele local a ete interroge.
*/
int agent_kernel_plan(const char* text, agent_call* out, int* used_llm);

/*
    Fenetre d'application ouverte par la derniere action open_app, ou -1.
    Lecture = remise a -1 : la barre Mikea se ferme alors et lui rend le focus.
*/
int agent_kernel_take_opened_slot(void);

#endif
