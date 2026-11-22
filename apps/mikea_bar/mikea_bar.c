#include "mikea_bar.h"
#include "mikea_bar_view.h"
#include "../../kernel/ai/agent.h"
#include "../../kernel/ai/agent_kernel.h"
#include "../../kernel/ai/jarvis.h"
#include "../../gui/gui.h"
#include "../../gui/theme.h"
#include "../../gui/desktop.h"
#include "../../gui/window.h"
#include "../../gui/ui.h"
#include "../../kernel/drivers/graphics/graphics.h"
#include "../../kernel/drivers/mouse/mouse.h"
#include "../../libc/string.h"

void console_write(const char* text);
int keyboard_available();
char keyboard_getchar();
unsigned long timer_ticks();

/*
    ============================================================
    Barre Mikea -- boucle d'evenements
    ============================================================

    La vue (mikea_bar_view.c) dessine ; ce fichier relie la saisie a
    l'agent (kernel/ai/agent_kernel.c) :

      saisie --Entree--> agent_kernel_plan() : regles hors ligne, sinon modele local
                              |
                              v
                        agent_decide() : garde-fous
                   RUN -------+------- CONFIRM / CONFIRM_STRONG
                    |                    |  affiche "Valider ?"
                    v                    v  (bouton ; Entree seulement si non destructive)
              agent_execute(call, confirmed) <-- SEUL point d'execution

    Appel bloquant au modele : pendant l'attente, la fenetre affiche
    "Mikea reflechit..." mais ne reagit pas (a ameliorer : fil de travail).
    ============================================================
*/

typedef struct
{
    bar_layout layout;
    bar_model model;
    ui_ctx ui;

    agent_call pending;     /* action en attente de confirmation */
    int have_pending;

    int redraw;             /* redessin complet (fond d'ecran, cadre) */
    int need_paint;         /* repeint du contenu */
    int suppress_edges;
    int close_requested;
    int focus_slot_after;   /* application a mettre au premier plan apres fermeture, ou -1 */

    s32 mx, my;
    int down;
} bar_ctx;

static const char* const SUGGEST_TEXT[BAR_SUGGESTIONS - 1] = {
    "Ouvre la calculatrice", "", "Infos système", "Liste mes fichiers"
};

static void bar_set_message(bar_ctx* c, const char* text, int ok)
{
    mk_strcpy(c->model.message, text, sizeof(c->model.message));
    c->model.message_ok = ok;
    c->model.state = BAR_RESULT;
}

static void bar_clear_input(bar_ctx* c)
{
    c->model.input[0] = 0;
}

/* Donnees qui peuvent changer a tout moment : relues a chaque repeint. */

static void bar_refresh(bar_ctx* c)
{
    bar_model* m = &c->model;

    m->llm_online = jarvis_llm_available();
    m->model_name = jarvis_get_model();
    m->can_undo = agent_undo_available();

    m->suggestion[0] = SUGGEST_TEXT[0];       m->suggestion_kind[0] = "Application";
    m->suggestion[1] = theme_is_dark() ? "Passe en mode clair" : "Passe en mode sombre";
    m->suggestion_kind[1] = "Réglage";
    m->suggestion[2] = SUGGEST_TEXT[2];       m->suggestion_kind[2] = "Info";
    m->suggestion[3] = SUGGEST_TEXT[3];       m->suggestion_kind[3] = "Fichiers";
    m->suggestion[4] = "Aide";                m->suggestion_kind[4] = "Aide";
}

static void bar_paint(bar_ctx* c, int down, bar_actions* act)
{
    bar_refresh(c);

    /* Restaure le fond sous le curseur avant de repeindre (il est redessine en fin de tour). */

    gui_cursor_erase();

    if (c->suppress_edges)
    {
        c->ui.prev_down = down;
        c->suppress_edges = 0;
    }

    ui_begin(&c->ui, c->mx, c->my, down);
    bar_view_paint(&c->ui, &c->layout, &c->model, act);
}

/* ------------------------------------------------------------
   Agent
   ------------------------------------------------------------ */

static void bar_run(bar_ctx* c, const agent_call* call, int confirmed)
{
    char out[320];

    int ok = agent_execute(call, confirmed, out, sizeof(out));

    bar_set_message(c, out, ok);
    c->have_pending = 0;
    c->model.confirm_strong = 0;

    /* Theme, fond d'ecran, annulation : tout l'ecran peut avoir change. */

    c->redraw = 1;

    if (ok && call->action >= 0)
    {
        const agent_action* a = agent_action_at((u32)call->action);

        if (a != (const agent_action*)0 && mk_strncmp(a->name, "open_app", 9) == 0)
        {
            int slot = agent_kernel_take_opened_slot();

            if (slot >= 0)
            {
                c->focus_slot_after = slot;
                c->close_requested = 1;
            }
        }
    }
}

static void bar_handle_call(bar_ctx* c, const agent_call* call)
{
    char reason[96];
    char out[320];

    c->model.from_llm = (call->source == AGENT_SRC_LLM);

    if (call->action == AGENT_ACT_REPLY)
    {
        bar_set_message(c, call->say, 1);
        return;
    }

    if (call->action == AGENT_ACT_UNDO)
    {
        int ok = agent_execute(call, 0, out, sizeof(out));

        bar_set_message(c, out, ok);
        c->redraw = 1;
        return;
    }

    agent_decision d = agent_decide(call, reason, sizeof(reason));

    if (d == AGENT_DECISION_REJECT)
    {
        bar_set_message(c, reason, 0);
        return;
    }

    if (d == AGENT_DECISION_RUN)
    {
        bar_run(c, call, 0);
        return;
    }

    /* Action sensible : on ne l'execute PAS, on demande. */

    c->pending = *call;
    c->have_pending = 1;
    c->model.confirm_strong = (d == AGENT_DECISION_CONFIRM_STRONG);
    c->model.message_ok = 1;
    c->model.state = BAR_CONFIRM;

    agent_describe(call, out, sizeof(out));
    mk_strcpy(c->model.message, out, sizeof(c->model.message));
}

static void bar_submit(bar_ctx* c)
{
    if (c->model.input[0] == 0)
    {
        return;
    }

    /* Montre "Mikea reflechit..." AVANT l'appel (qui peut bloquer longtemps). */

    c->model.state = BAR_THINKING;

    {
        bar_actions ignored;

        bar_paint(c, 0, &ignored);
        gui_draw_cursor(c->mx, c->my);
    }

    agent_call call;
    int used_llm = 0;

    int got = agent_kernel_plan(c->model.input, &call, &used_llm);

    if (!got)
    {
        c->model.state = BAR_IDLE;
        return;
    }

    bar_handle_call(c, &call);
    bar_clear_input(c);
}

static void bar_cancel_pending(bar_ctx* c)
{
    c->have_pending = 0;
    c->model.confirm_strong = 0;
    c->model.from_llm = 0;
    bar_set_message(c, "D'accord, je n'ai rien fait.", 1);
}

/* Journal -> "Recent" (non affiche pour l'instant, garde le modele complet). */

static void bar_load_recent(bar_ctx* c)
{
    c->model.recent_count = 0;

    for (u32 i = 0; i < 3; i++)
    {
        const agent_log_entry* e = agent_log_at(i);

        if (e == (const agent_log_entry*)0) { break; }

        const agent_action* a = (e->action >= 0) ? agent_action_at((u32)e->action) : (const agent_action*)0;

        mk_strcpy(c->model.recent[c->model.recent_count], a != (const agent_action*)0 ? a->name : "?", 48);
        c->model.recent_count++;
    }
}

/* ------------------------------------------------------------
   Boucle principale
   ------------------------------------------------------------ */

void cmd_mikea_bar()
{
    static bar_ctx ctx;   /* statique : trop gros pour la pile d'un fil du noyau */

    if (!gfx_available())
    {
        console_write("La barre Mikea necessite le mode graphique et une souris.\n");
        return;
    }

    int slot = gui_window_claim_slot();

    if (slot < 0)
    {
        return;
    }

    bar_ctx* c = &ctx;

    mk_memset(c, 0, sizeof(*c));
    agent_kernel_init();

    c->model.state = BAR_IDLE;
    c->focus_slot_after = -1;
    c->redraw = 1;

    int was_pressed = 0;
    int last_hover = 0;

    while (1)
    {
        if (!gui_window_has_focus(slot))
        {
            c->redraw = 1;
            gui_window_idle();
            continue;
        }

        if (c->redraw)
        {
            desktop_render_backdrop();

            bar_view_layout(&c->layout, (int)gfx_width(), (int)gfx_height());
            bar_view_paint_frame(&c->layout);

            was_pressed = mouse_left_pressed();
            c->redraw = 0;
            c->need_paint = 1;
            c->suppress_edges = 1;
        }

        c->mx = mouse_get_x();
        c->my = mouse_get_y();

        int now_pressed = mouse_left_pressed();
        int clicked = (now_pressed && !was_pressed);
        int released = (!now_pressed && was_pressed);

        was_pressed = now_pressed;
        c->down = now_pressed;

        /* Un clic dans la bande de la barre des taches appartient au dock. */

        if (clicked && (u32)c->my >= desktop_dock_top_px())
        {
            gui_cursor_erase();
            gui_window_yield_click(slot);
            continue;
        }

        /* Clic en dehors du panneau : on ferme, comme Spotlight. */

        if (clicked)
        {
            ui_rect p = c->layout.panel;

            if (c->mx < p.x || c->my < p.y || c->mx >= p.x + p.w || c->my >= p.y + p.h)
            {
                c->close_requested = 1;
            }
        }

        /* ---- Clavier ---- */

        while (keyboard_available() && !c->close_requested)
        {
            char ch = keyboard_getchar();
            bar_model* m = &c->model;

            c->need_paint = 1;

            if (ch == 27)                                   /* Echap */
            {
                if (m->state == BAR_CONFIRM) { bar_cancel_pending(c); }
                else { c->close_requested = 1; }
            }
            else if (ch == '\n')
            {
                if (m->state == BAR_CONFIRM)
                {
                    /* Entree confirme une action modificatrice, JAMAIS une action destructive : il faut cliquer. */

                    if (!m->confirm_strong && c->have_pending) { bar_run(c, &c->pending, 1); }
                }
                else
                {
                    bar_submit(c);
                }
            }
            else if (m->state == BAR_CONFIRM)
            {
                if (ch == 'n' || ch == 'N') { bar_cancel_pending(c); }
            }
            else if (ch == '\b' || ch == 127)
            {
                u32 n = (u32)mk_strlen(m->input);

                if (n > 0) { m->input[n - 1] = 0; }

                if (m->state == BAR_RESULT) { m->state = BAR_IDLE; }
            }
            else if (ch >= 32 && ch < 127)
            {
                u32 n = (u32)mk_strlen(m->input);

                if (n < sizeof(m->input) - 1)
                {
                    m->input[n] = ch;
                    m->input[n + 1] = 0;
                }

                if (m->state == BAR_RESULT) { m->state = BAR_IDLE; }
            }
        }

        /* ---- Contenu : repeint partiel si survol, clic, saisie ou etat change ---- */

        if (!c->redraw && !c->close_requested)
        {
            int hover = bar_view_hit(&c->layout, &c->model, c->mx, c->my);

            if (c->need_paint || hover != last_hover || clicked || released)
            {
                bar_actions act;

                bar_load_recent(c);
                bar_paint(c, now_pressed, &act);

                last_hover = hover;
                c->need_paint = 0;

                if (act.suggestion >= 0)
                {
                    mk_strcpy(c->model.input, c->model.suggestion[act.suggestion], sizeof(c->model.input));
                    bar_submit(c);
                    c->need_paint = 1;
                }
                else if (act.confirm && c->have_pending)
                {
                    bar_run(c, &c->pending, 1);
                    c->need_paint = 1;
                }
                else if (act.cancel)
                {
                    bar_cancel_pending(c);
                    c->need_paint = 1;
                }
                else if (act.undo)
                {
                    char out[320];
                    int ok = agent_undo_last(out, sizeof(out));

                    bar_set_message(c, out, ok);
                    c->model.from_llm = 0;
                    c->redraw = 1;
                }
            }
        }

        if (c->close_requested)
        {
            gui_cursor_erase();
            gui_window_close(slot);

            /* L'application lancee par l'agent prend le focus, pas la fenetre d'avant. */

            if (c->focus_slot_after >= 0) { gui_window_focus(c->focus_slot_after); }

            break;
        }

        gui_draw_cursor(c->mx, c->my);

        unsigned long frame_start = timer_ticks();

        while (timer_ticks() - frame_start < 1)
        {
        }
    }
}
