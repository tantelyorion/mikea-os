#include "mikea_bar_view.h"
#include "../../gui/theme.h"

#define PANEL_H 336

static u32 U(s32 v) { return v < 0 ? 0u : (u32)v; }

static int in_rect(ui_rect r, s32 x, s32 y)
{
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static gfx_color dim(void)
{
    return ui_mix(theme_text(), theme_panel(), 45);
}

/* ------------------------------------------------------------
   Disposition
   ------------------------------------------------------------ */

void bar_view_layout(bar_layout* L, int screen_w, int screen_h)
{
    s32 w = screen_w - 48;

    if (w > 640) { w = 640; }
    if (w < 320) { w = 320; }

    s32 y = screen_h / 8;

    if (y < 40) { y = 40; }

    L->panel = (ui_rect){ (screen_w - w) / 2, y, w, PANEL_H };

    ui_rect in = ui_inset(L->panel, 14);

    L->field = ui_cut_top(&in, 52);
    ui_cut_top(&in, 12);

    ui_rect footer = ui_cut_bottom(&in, 22);
    ui_cut_bottom(&in, 8);

    L->footer_status = ui_cut_right(&footer, 220);
    L->footer_hint = footer;

    L->body = in;

    ui_rect rows = in;

    for (int i = 0; i < BAR_SUGGESTIONS; i++)
    {
        L->suggestion[i] = ui_cut_top(&rows, 36);
    }

    /* Resultat / confirmation : carte en haut du corps, boutons en bas. */

    ui_rect b = in;
    ui_rect buttons = ui_cut_bottom(&b, UI_BTN_H);
    ui_cut_bottom(&b, 8);

    L->message = b;

    L->btn_confirm = ui_cut_right(&buttons, 130);
    ui_cut_right(&buttons, 8);
    L->btn_cancel = ui_cut_right(&buttons, 110);
    L->btn_undo = ui_cut_left(&buttons, 230);
}

void bar_view_paint_frame(const bar_layout* L)
{
    ui_card(L->panel);
}

int bar_view_hit(const bar_layout* L, const bar_model* m, s32 x, s32 y)
{
    if (m->state == BAR_IDLE)
    {
        for (int i = 0; i < BAR_SUGGESTIONS; i++)
        {
            if (m->suggestion[i] != (const char*)0 && in_rect(L->suggestion[i], x, y)) { return BAR_ID_SUGGESTION0 + i; }
        }
    }

    if (m->state == BAR_CONFIRM)
    {
        if (in_rect(L->btn_confirm, x, y)) { return BAR_ID_CONFIRM; }
        if (in_rect(L->btn_cancel, x, y)) { return BAR_ID_CANCEL; }
    }

    if ((m->state == BAR_RESULT || m->state == BAR_IDLE) && m->can_undo && in_rect(L->btn_undo, x, y))
    {
        return BAR_ID_UNDO;
    }

    return 0;
}

/* ------------------------------------------------------------
   Texte
   ------------------------------------------------------------ */

/* Ecrit "text" a la ligne, en passant a la ligne aux espaces. Renvoie le nombre de lignes. */

static int draw_wrapped(ui_rect r, const char* text, int font, gfx_color color, int max_lines)
{
    s32 line_h = (s32)gfx_font_height(font) + 2;
    int lines = 0;
    u32 i = 0;

    while (text[i] != 0 && lines < max_lines)
    {
        char line[160];
        u32 n = 0;
        u32 last_space = 0;
        int have_space = 0;
        u32 j = i;

        /* Avance mot par mot tant que la ligne tient dans la largeur. */

        while (text[j] != 0 && n < sizeof(line) - 1)
        {
            line[n] = text[j];
            line[n + 1] = 0;

            if (text[j] == ' ') { last_space = n; have_space = 1; }

            if ((s32)gfx_text_width(line, font) > r.w && n > 0)
            {
                if (have_space)
                {
                    n = last_space;      /* coupe au dernier espace */
                    j = i + n + 1;
                }

                break;
            }

            n++;
            j++;
        }

        line[n] = 0;
        i = j;

        while (text[i] == ' ') { i++; }

        /* Derniere ligne autorisee et texte restant : on l'indique par une ellipse. */

        if (lines == max_lines - 1 && text[i] != 0 && n + 3 < sizeof(line))
        {
            line[n++] = '.'; line[n++] = '.'; line[n++] = '.'; line[n] = 0;
        }

        gfx_draw_text_ui(U(r.x), U(r.y + lines * line_h), line, color, font);
        lines++;
    }

    return lines;
}

static void dot(s32 cx, s32 cy, s32 d, gfx_color c)
{
    gfx_fill_rounded_rect_aa(U(cx - d / 2), U(cy - d / 2), U(d), U(d), U(d / 2), c, 100);
}

/* ------------------------------------------------------------
   Dessin
   ------------------------------------------------------------ */

static void paint_field(ui_ctx* ui, const bar_layout* L, const bar_model* m)
{
    /* On n'affiche que la fin du texte s'il depasse : le curseur reste visible. */

    char shown[96];
    u32 start = 0;

    while (m->input[start] != 0 && (s32)gfx_text_width(m->input + start, GFX_FONT_BODY) > L->field.w - 52) { start++; }

    u32 n = 0;

    while (m->input[start + n] != 0 && n < sizeof(shown) - 1) { shown[n] = m->input[start + n]; n++; }

    shown[n] = 0;

    ui_text_field(ui, 1, L->field, shown, "Demandez quelque chose à Mikea…", 1, 0);
}

static void paint_message(const bar_layout* L, const bar_model* m)
{
    gfx_color tint = m->message_ok ? ui_mix(theme_panel(), theme_accent(), 12) : ui_mix(theme_panel(), theme_close_bg(), 14);
    gfx_color edge = m->message_ok ? ui_mix(theme_panel(), theme_accent(), 35) : ui_mix(theme_panel(), theme_close_bg(), 40);

    ui_rect card = L->message;

    gfx_fill_rounded_rect_aa(U(card.x), U(card.y), U(card.w), U(card.h), UI_RADIUS, edge, 100);
    gfx_fill_rounded_rect_aa(U(card.x + 1), U(card.y + 1), U(card.w - 2), U(card.h - 2), UI_RADIUS - 1, tint, 100);

    ui_rect in = ui_inset(card, 14);

    if (m->state == BAR_THINKING)
    {
        dot(in.x + 6, in.y + 11, 12, dim());
        ui_rect t = in;
        t.x += 26;
        t.w -= 26;
        draw_wrapped(t, "Mikea réfléchit…", GFX_FONT_BODY, theme_text(), 1);
        return;
    }

    gfx_color mark = (m->state == BAR_CONFIRM) ? theme_accent() : (m->message_ok ? 0x30B050 : theme_close_bg());

    dot(in.x + 6, in.y + 11, 12, mark);

    ui_rect t = in;
    t.x += 26;
    t.w -= 26;

    int lines = draw_wrapped(t, m->message, GFX_FONT_BODY, theme_text(), 5);

    /* Provenance de la decision, discretement sous le texte. */

    if (m->state == BAR_RESULT)
    {
        char who[96];
        u32 n = 0;
        const char* a = m->from_llm ? "Compris par l’IA locale" : "Compris par les règles (hors ligne)";

        for (u32 i = 0; a[i] != 0 && n < sizeof(who) - 1; i++) { who[n++] = a[i]; }

        if (m->from_llm && m->model_name != (const char*)0)
        {
            const char* b = " · ";

            for (u32 i = 0; b[i] != 0 && n < sizeof(who) - 1; i++) { who[n++] = b[i]; }
            for (u32 i = 0; m->model_name[i] != 0 && n < sizeof(who) - 1; i++) { who[n++] = m->model_name[i]; }
        }

        who[n] = 0;

        s32 y = t.y + lines * ((s32)gfx_font_height(GFX_FONT_BODY) + 2) + 6;

        if (y + 14 < card.y + card.h) { gfx_draw_text_ui(U(t.x), U(y), who, dim(), GFX_FONT_SMALL); }
    }

    if (m->state == BAR_CONFIRM)
    {
        gfx_draw_text_ui(U(t.x), U(card.y + card.h - 24),
                         m->confirm_strong ? "Action sensible : récupérable depuis la corbeille" : "Cette action modifie vos données",
                         dim(), GFX_FONT_SMALL);
    }
}

void bar_view_paint(ui_ctx* ui, const bar_layout* L, const bar_model* m, bar_actions* out)
{
    out->suggestion = -1;
    out->confirm = 0;
    out->cancel = 0;
    out->undo = 0;

    ui_card_clear(L->panel);

    paint_field(ui, L, m);

    if (m->state == BAR_IDLE)
    {
        for (int i = 0; i < BAR_SUGGESTIONS; i++)
        {
            if (m->suggestion[i] == (const char*)0) { continue; }

            if (ui_list_row(ui, BAR_ID_SUGGESTION0 + i, L->suggestion[i], m->suggestion[i], m->suggestion_kind[i], 0))
            {
                out->suggestion = i;
            }
        }
    }
    else
    {
        paint_message(L, m);
    }

    if (m->state == BAR_CONFIRM)
    {
        if (ui_button(ui, BAR_ID_CANCEL, L->btn_cancel, "Annuler", UI_BTN_NORMAL)) { out->cancel = 1; }

        if (ui_button(ui, BAR_ID_CONFIRM, L->btn_confirm, "Valider", m->confirm_strong ? UI_BTN_DANGER : UI_BTN_PRIMARY))
        {
            out->confirm = 1;
        }
    }
    else if ((m->state == BAR_RESULT || m->state == BAR_IDLE) && m->can_undo)
    {
        if (ui_button(ui, BAR_ID_UNDO, L->btn_undo, "Annuler la dernière action", UI_BTN_NORMAL)) { out->undo = 1; }
    }

    /* Pied : aide a gauche, etat du modele a droite. */

    ui_label(L->footer_hint, "Entrée : envoyer  ·  Échap : fermer", GFX_FONT_SMALL, dim(), UI_ALIGN_LEFT);

    {
        const char* name = (m->model_name != (const char*)0) ? m->model_name : "?";
        char short_name[32];
        char status[80];

        /* Nom long (ex. "hf.co/Org/Depot:Q4_K_M") : on garde la FIN, la plus parlante, precedee de "...". */

        {
            u32 len = 0;

            while (name[len] != 0) { len++; }

            if (len <= 26)
            {
                for (u32 i = 0; i <= len; i++) { short_name[i] = name[i]; }
            }
            else
            {
                const char* tail = name + (len - 24);
                u32 k = 0;

                short_name[k++] = '.'; short_name[k++] = '.'; short_name[k++] = '.';

                for (u32 i = 0; tail[i] != 0 && k < sizeof(short_name) - 1; i++) { short_name[k++] = tail[i]; }

                short_name[k] = 0;
            }

            name = short_name;
        }

        u32 n = 0;
        const char* a = m->llm_online ? "IA locale : " : "Hors ligne : ";

        for (u32 i = 0; a[i] != 0 && n < sizeof(status) - 1; i++) { status[n++] = a[i]; }
        for (u32 i = 0; name[i] != 0 && n < sizeof(status) - 1; i++) { status[n++] = name[i]; }

        status[n] = 0;

        s32 w = (s32)gfx_text_width(status, GFX_FONT_SMALL);
        ui_rect text_r = L->footer_status;

        ui_label(text_r, status, GFX_FONT_SMALL, dim(), UI_ALIGN_RIGHT);
        dot(text_r.x + text_r.w - w - 10, text_r.y + text_r.h / 2, 8, m->llm_online ? 0x30B050 : ui_mix(theme_text(), theme_panel(), 60));
    }
}
