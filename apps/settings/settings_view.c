#include "settings_view.h"
#include "../../gui/theme.h"
#include "../../gui/desktop.h"

#define TITLEBAR_H 20   /* 8 * GFX_SCALE + 4, voir gui_draw_window() */
#define CELL_PX    16   /* 8 * GFX_SCALE */

static u32 U(s32 v)
{
    return v < 0 ? 0u : (u32)v;
}

static int in_rect(ui_rect r, s32 x, s32 y)
{
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

/* ------------------------------------------------------------
   Petits utilitaires de texte (pas de libc : voir shell/msh.c)
   ------------------------------------------------------------ */

static void sb_add(char* b, int cap, const char* s)
{
    int n = 0;

    while (b[n] != 0) { n++; }

    for (int i = 0; s[i] != 0 && n < cap - 1; i++)
    {
        b[n++] = s[i];
    }

    b[n] = 0;
}

static void sb_add_u32(char* b, int cap, u32 v, int pad2)
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
    sb_add(b, cap, out);
}

static gfx_color dim_text(void)
{
    return ui_mix(theme_text(), theme_panel(), 45);
}

/* ------------------------------------------------------------
   Disposition
   ------------------------------------------------------------ */

void settings_view_layout(settings_layout* L, int win_x, int win_y, int win_w, int win_h)
{
    s32 wx = win_x * CELL_PX;
    s32 wy = win_y * CELL_PX;
    s32 ww = win_w * CELL_PX;
    s32 wh = win_h * CELL_PX;

    ui_rect area = { wx + 12, wy + TITLEBAR_H + 12, ww - 24, wh - TITLEBAR_H - 24 };

    /* Pied de fenetre ancre en bas : carte opaque (repeints partiels idempotents). */

    L->card_footer = ui_cut_bottom(&area, UI_BTN_H + 20);
    ui_cut_bottom(&area, 12);

    ui_rect footer = ui_inset(L->card_footer, 10);

    L->logout_btn = ui_cut_right(&footer, 150);
    L->hint = footer;
    L->hint.x += 4;

    /* Rangee 1 : colonnes (haut) ; rangee 2 : bandeau systeme. */

    s32 row1_h = area.h - 84 - 12;

    if (row1_h < 204) { row1_h = 204; }
    if (row1_h > 300) { row1_h = 300; }

    ui_rect row1 = ui_cut_top(&area, row1_h);
    ui_cut_top(&area, 12);
    L->card_system = ui_cut_top(&area, 84);

    ui_rect left = ui_cut_left(&row1, row1.w / 2 - 6);
    ui_cut_left(&row1, 12);
    ui_rect right = row1;

    L->card_session = ui_cut_top(&left, 62);
    ui_cut_top(&left, 12);
    L->card_appearance = left;
    L->card_wallpaper = right;

    /* Apparence : deux lignes (libelle a gauche, bascule a droite). */

    ui_rect ai = ui_inset(L->card_appearance, 12);
    ui_cut_top(&ai, 24);

    ui_rect r1 = ui_cut_top(&ai, UI_ROW_H);
    ui_rect r2 = ui_cut_top(&ai, UI_ROW_H);

    L->theme_toggle = (ui_rect){ r1.x + r1.w - 44, r1.y + (r1.h - 26) / 2, 44, 26 };
    L->sound_toggle = (ui_rect){ r2.x + r2.w - 44, r2.y + (r2.h - 26) / 2, 44, 26 };

    /* Fond d'ecran : bouton "motif" en bas, vignettes 3 x 2 au-dessus. */

    ui_rect wi = ui_inset(L->card_wallpaper, 12);
    ui_cut_top(&wi, 24);
    L->pattern_btn = ui_cut_bottom(&wi, UI_BTN_H);
    ui_cut_bottom(&wi, 8);

    s32 tw = (wi.w - 16) / 3;
    s32 th = (wi.h - 8) / 2;

    if (th * 4 / 3 < tw) { tw = th * 4 / 3; }       /* ratio 4:3 des photos */

    for (int i = 0; i < SETTINGS_SWATCH_COUNT; i++)
    {
        int col = i % 3;
        int row = i / 3;

        L->swatch[i] = (ui_rect){ wi.x + col * (tw + 8), wi.y + row * (tw * 3 / 4 + 8), tw, tw * 3 / 4 };
    }
}

void settings_view_paint_cards(const settings_layout* L)
{
    ui_card(L->card_session);
    ui_card(L->card_appearance);
    ui_card(L->card_wallpaper);
    ui_card(L->card_system);
    ui_card(L->card_footer);
}

int settings_view_hit(const settings_layout* L, s32 x, s32 y)
{
    if (in_rect(L->theme_toggle, x, y)) { return SETTINGS_ID_THEME; }
    if (in_rect(L->sound_toggle, x, y)) { return SETTINGS_ID_SOUND; }
    if (in_rect(L->pattern_btn, x, y))  { return SETTINGS_ID_PATTERN; }
    if (in_rect(L->logout_btn, x, y))   { return SETTINGS_ID_LOGOUT; }

    for (int i = 0; i < SETTINGS_SWATCH_COUNT; i++)
    {
        if (in_rect(L->swatch[i], x, y)) { return SETTINGS_ID_SWATCH0 + i; }
    }

    return 0;
}

/* ------------------------------------------------------------
   Dessin
   ------------------------------------------------------------ */

static void caption(ui_rect r, const char* text)
{
    ui_label(r, text, GFX_FONT_SMALL, dim_text(), UI_ALIGN_LEFT);
}

static void paint_session(const settings_layout* L, const settings_model* m)
{
    ui_rect in = ui_inset(L->card_session, 12);
    ui_rect avatar = ui_cut_left(&in, 38);
    ui_cut_left(&in, 12);

    avatar.y += (avatar.h - 38) / 2;
    avatar.h = 38;
    avatar.w = 38;

    gfx_fill_rounded_rect_aa(U(avatar.x), U(avatar.y), 38, 38, 19, theme_accent(), 100);

    char initial[2] = { '?', 0 };

    if (m->username != (const char*)0 && m->username[0] != 0)
    {
        char c = m->username[0];
        initial[0] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }

    ui_label(avatar, initial, GFX_FONT_BODY, theme_accent_text(), UI_ALIGN_CENTER);

    ui_rect line1 = ui_cut_top(&in, in.h / 2);

    if (m->username == (const char*)0)
    {
        ui_label(in, "Aucun utilisateur connecté", GFX_FONT_BODY, theme_text(), UI_ALIGN_LEFT);
        return;
    }

    ui_label(line1, m->username, GFX_FONT_BODY, theme_text(), UI_ALIGN_LEFT);
    ui_label(in, m->is_root ? "Administrateur (root)" : "Utilisateur standard", GFX_FONT_SMALL, dim_text(), UI_ALIGN_LEFT);
}

static void paint_appearance(ui_ctx* ui, const settings_layout* L, const settings_model* m, settings_actions* out)
{
    ui_rect ai = ui_inset(L->card_appearance, 12);

    caption(ui_cut_top(&ai, 24), "Apparence");

    ui_rect r1 = ui_cut_top(&ai, UI_ROW_H);
    ui_rect r2 = ui_cut_top(&ai, UI_ROW_H);

    ui_label(r1, "Mode sombre", GFX_FONT_BODY, theme_text(), UI_ALIGN_LEFT);
    ui_label(r2, "Sons du système", GFX_FONT_BODY, theme_text(), UI_ALIGN_LEFT);

    int dark = m->dark;
    int sound = m->sound_on;

    if (ui_toggle(ui, SETTINGS_ID_THEME, L->theme_toggle, &dark))  { out->toggle_theme = 1; }
    if (ui_toggle(ui, SETTINGS_ID_SOUND, L->sound_toggle, &sound)) { out->toggle_sound = 1; }
}

static void paint_wallpaper(ui_ctx* ui, const settings_layout* L, const settings_model* m, settings_actions* out)
{
    ui_rect wi = ui_inset(L->card_wallpaper, 12);

    caption(ui_cut_top(&wi, 24), "Fond d’écran");

    for (int i = 0; i < SETTINGS_SWATCH_COUNT; i++)
    {
        ui_rect s = L->swatch[i];
        int hover = 0, held = 0;

        if (ui_region(ui, SETTINGS_ID_SWATCH0 + i, s, &hover, &held))
        {
            out->wallpaper = i;
        }

        /* Fond neutre sous la photo (visible si le pack d'assets est absent). */

        gfx_fill_rect(U(s.x), U(s.y), U(s.w), U(s.h), ui_mix(theme_panel(), theme_border(), 60));
        gui_paint_photo_area(U(s.x), U(s.y), U(s.w), U(s.h), (u32)i);

        if (i == m->photo_index)
        {
            gfx_draw_rect(U(s.x - 2), U(s.y - 2), U(s.w + 4), U(s.h + 4), theme_accent());
            gfx_draw_rect(U(s.x - 3), U(s.y - 3), U(s.w + 6), U(s.h + 6), theme_accent());
        }
        else if (hover)
        {
            gfx_draw_rect(U(s.x - 2), U(s.y - 2), U(s.w + 4), U(s.h + 4), ui_mix(theme_border(), theme_text(), 40));
        }
    }

    char label[48];

    label[0] = 0;
    sb_add(label, 48, "Motif : ");
    sb_add(label, 48, m->pattern_name != (const char*)0 ? m->pattern_name : "-");

    if (ui_button(ui, SETTINGS_ID_PATTERN, L->pattern_btn, label, UI_BTN_NORMAL))
    {
        out->pattern = 1;
    }
}

static void stat_column(ui_rect col, const char* title, const char* value, int percent)
{
    caption(ui_cut_top(&col, 20), title);
    ui_label(ui_cut_top(&col, 22), value, GFX_FONT_SMALL, theme_text(), UI_ALIGN_LEFT);

    if (percent >= 0)
    {
        ui_cut_top(&col, 6);
        ui_progress(ui_cut_top(&col, 8), (u32)percent);
    }
}

static void paint_system(const settings_layout* L, const settings_model* m)
{
    ui_rect in = ui_inset(L->card_system, 14);
    s32 cw = (in.w - 32) / 3;

    ui_rect c1 = ui_cut_left(&in, cw);  ui_cut_left(&in, 16);
    ui_rect c2 = ui_cut_left(&in, cw);  ui_cut_left(&in, 16);
    ui_rect c3 = in;

    char v[64];

    /* Disque */

    u32 d_used = m->disk_total_kb > m->disk_free_kb ? m->disk_total_kb - m->disk_free_kb : 0;
    int d_pct = m->disk_total_kb ? (int)((d_used * 100) / m->disk_total_kb) : 0;

    v[0] = 0;
    sb_add_u32(v, 64, m->disk_free_kb, 0);
    sb_add(v, 64, " Ko libres sur ");
    sb_add_u32(v, 64, m->disk_total_kb, 0);
    sb_add(v, 64, " Ko");
    stat_column(c1, "Disque", v, d_pct);

    /* Memoire (tas du noyau) */

    int r_pct = m->heap_total_kb ? (int)((m->heap_used_kb * 100) / m->heap_total_kb) : 0;

    v[0] = 0;
    sb_add_u32(v, 64, m->heap_used_kb, 0);
    sb_add(v, 64, " / ");
    sb_add_u32(v, 64, m->heap_total_kb, 0);
    sb_add(v, 64, " Ko utilisés");
    stat_column(c2, "Mémoire", v, r_pct);

    /* Date et heure */

    v[0] = 0;
    sb_add_u32(v, 64, m->day, 1);
    sb_add(v, 64, "/");
    sb_add_u32(v, 64, m->month, 1);
    sb_add(v, 64, "/");
    sb_add_u32(v, 64, m->year, 0);
    sb_add(v, 64, "  ");
    sb_add_u32(v, 64, m->hour, 1);
    sb_add(v, 64, ":");
    sb_add_u32(v, 64, m->minute, 1);
    stat_column(c3, "Date et heure", v, -1);
}

void settings_view_paint(ui_ctx* ui, const settings_layout* L, const settings_model* m, settings_actions* out)
{
    out->toggle_theme = 0;
    out->toggle_sound = 0;
    out->wallpaper = -1;
    out->pattern = 0;
    out->logout = 0;

    /* Fond connu sous les widgets (voir ui_card_clear). */

    ui_card_clear(L->card_session);
    ui_card_clear(L->card_appearance);
    ui_card_clear(L->card_wallpaper);
    ui_card_clear(L->card_system);
    ui_card_clear(L->card_footer);

    paint_session(L, m);
    paint_appearance(ui, L, m, out);
    paint_wallpaper(ui, L, m, out);
    paint_system(L, m);

    ui_label(L->hint, "Entrée : fermer", GFX_FONT_SMALL, dim_text(), UI_ALIGN_LEFT);

    if (ui_button(ui, SETTINGS_ID_LOGOUT, L->logout_btn, "Déconnexion", UI_BTN_DANGER))
    {
        out->logout = 1;
    }
}
