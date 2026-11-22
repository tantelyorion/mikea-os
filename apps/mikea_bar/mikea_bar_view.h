#ifndef MIKEA_APP_BAR_VIEW_H
#define MIKEA_APP_BAR_VIEW_H

#include "../../gui/ui.h"

/*
    ============================================================
    Barre Mikea -- vue (mise en page + dessin en pixels)
    ============================================================

    Zone de saisie en langage naturel (type Spotlight) pilotee par
    l'agent IA (kernel/ai/agent*.c). Comme Reglages, la vue est separee
    de la boucle d'evenements et ne depend que de gui/ui : elle se teste
    sur l'hote (tools/tests/mikea_bar_view_host_test.c).

        +--------------------------------------------------------+
        | > Demandez quelque chose a Mikea...                    |
        +--------------------------------------------------------+
        |  [etat IDLE]      suggestions cliquables               |
        |  [etat THINKING]  "Mikea reflechit..."                 |
        |  [etat RESULT]    carte de resultat + Annuler l'action |
        |  [etat CONFIRM]   "Creer le fichier x ?" [Valider][Non]|
        +--------------------------------------------------------+
        | Entree : envoyer - Echap : fermer        o modele ...  |
        +--------------------------------------------------------+
    ============================================================
*/

typedef enum
{
    BAR_IDLE = 0,       /* saisie + suggestions */
    BAR_THINKING = 1,   /* le modele travaille */
    BAR_RESULT = 2,     /* resultat d'une action / reponse */
    BAR_CONFIRM = 3     /* action sensible : attend Valider / Annuler */
} bar_state;

#define BAR_SUGGESTIONS 5

typedef struct
{
    bar_state state;

    char input[96];

    char message[320];          /* resultat, reponse ou question de confirmation */
    int  message_ok;            /* 1 = succes, 0 = erreur / refus */
    int  confirm_strong;        /* 1 = action destructive : bouton rouge */
    int  from_llm;              /* 1 = compris par le modele local, 0 = regles hors ligne */

    int  can_undo;
    int  llm_online;
    const char* model_name;

    const char* suggestion[BAR_SUGGESTIONS];
    const char* suggestion_kind[BAR_SUGGESTIONS];

    char recent[3][48];         /* dernieres actions, la plus recente d'abord */
    int  recent_count;
} bar_model;

typedef struct
{
    ui_rect panel;
    ui_rect field;
    ui_rect body;
    ui_rect suggestion[BAR_SUGGESTIONS];
    ui_rect message;
    ui_rect btn_confirm, btn_cancel, btn_undo;
    ui_rect footer_hint, footer_status;
} bar_layout;

typedef struct
{
    int suggestion;     /* indice clique, ou -1 */
    int confirm;
    int cancel;
    int undo;
} bar_actions;

#define BAR_ID_SUGGESTION0 10    /* 10 a 14 */
#define BAR_ID_CONFIRM     20
#define BAR_ID_CANCEL      21
#define BAR_ID_UNDO        22

void bar_view_layout(bar_layout* L, int screen_w, int screen_h);

/* Cadre avec ombre : seulement lors d'un redessin COMPLET. */
void bar_view_paint_frame(const bar_layout* L);

/* Contenu : repeint partiel possible a tout moment (fond connu, idempotent). */
void bar_view_paint(ui_ctx* ui, const bar_layout* L, const bar_model* m, bar_actions* out);

/* Widget sous (x, y), 0 si aucun : detecte un changement de survol. */
int bar_view_hit(const bar_layout* L, const bar_model* m, s32 x, s32 y);

#endif
