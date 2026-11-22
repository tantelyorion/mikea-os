#include "ui.h"
#include "theme.h"

/* ------------------------------------------------------------
   Utilitaires
   ------------------------------------------------------------ */

gfx_color ui_mix(gfx_color a, gfx_color b, u32 t)
{
    if (t > 100) { t = 100; }

    u32 ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    u32 br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;

    u32 r = (ar * (100 - t) + br * t) / 100;
    u32 g = (ag * (100 - t) + bg * t) / 100;
    u32 bl = (ab * (100 - t) + bb * t) / 100;

    return (r << 16) | (g << 8) | bl;
}

static int ui_hit(ui_rect r, s32 x, s32 y)
{
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

/* Les fonctions du pilote prennent des u32 : on borne a 0 pour ne jamais passer un negatif. */

static u32 U(s32 v)
{
    return v < 0 ? 0u : (u32)v;
}

static void ui_round(ui_rect r, u32 radius, gfx_color color)
{
    if (r.w > 0 && r.h > 0)
    {
        gfx_fill_rounded_rect_aa(U(r.x), U(r.y), U(r.w), U(r.h), radius, color, 100);
    }
}

/* Panneau avec fine bordure : bordure pleine puis interieur plus petit. */

static void ui_round_bordered(ui_rect r, u32 radius, gfx_color fill, gfx_color border)
{
    ui_round(r, radius, border);
    ui_round(ui_inset(r, 1), radius > 0 ? radius - 1 : 0, fill);
}

/* ------------------------------------------------------------
   Mise en page
   ------------------------------------------------------------ */

ui_rect ui_inset(ui_rect r, s32 d)
{
    ui_rect o = { r.x + d, r.y + d, r.w - 2 * d, r.h - 2 * d };

    if (o.w < 0) { o.w = 0; }
    if (o.h < 0) { o.h = 0; }

    return o;
}

ui_rect ui_cut_top(ui_rect* r, s32 h)
{
    if (h > r->h) { h = r->h; }

    ui_rect s = { r->x, r->y, r->w, h };

    r->y += h;
    r->h -= h;

    return s;
}

ui_rect ui_cut_bottom(ui_rect* r, s32 h)
{
    if (h > r->h) { h = r->h; }

    ui_rect s = { r->x, r->y + r->h - h, r->w, h };

    r->h -= h;

    return s;
}

ui_rect ui_cut_left(ui_rect* r, s32 w)
{
    if (w > r->w) { w = r->w; }

    ui_rect s = { r->x, r->y, w, r->h };

    r->x += w;
    r->w -= w;

    return s;
}

ui_rect ui_cut_right(ui_rect* r, s32 w)
{
    if (w > r->w) { w = r->w; }

    ui_rect s = { r->x + r->w - w, r->y, w, r->h };

    r->w -= w;

    return s;
}

/* ------------------------------------------------------------
   Etat de la souris
   ------------------------------------------------------------ */

void ui_begin(ui_ctx* c, s32 mouse_x, s32 mouse_y, int left_down)
{
    c->mouse_x = mouse_x;
    c->mouse_y = mouse_y;
    c->down = left_down ? 1 : 0;
    c->pressed = (c->down && !c->prev_down) ? 1 : 0;
    c->released = (!c->down && c->prev_down) ? 1 : 0;
    c->prev_down = c->down;
    c->hot = 0;

    /* Bouton relache hors de tout widget : plus de clic en cours. */

    if (!c->down && !c->released)
    {
        c->active = 0;
    }
}

/*
    Logique commune a tous les widgets cliquables. Renvoie 1 au
    relachement du bouton SUR le widget qui avait recu l'appui.
    *hover / *held servent au dessin.
*/

static int ui_interact(ui_ctx* c, u32 id, ui_rect r, int* hover, int* held)
{
    int over = ui_hit(r, c->mouse_x, c->mouse_y);
    int clicked = 0;

    if (over)
    {
        c->hot = id;
    }

    if (c->active == id)
    {
        if (c->released)
        {
            clicked = over;
            c->active = 0;
        }
    }
    else if (over && c->pressed && c->active == 0)
    {
        c->active = id;
    }

    *hover = over;
    *held = (c->active == id) && over;

    return clicked;
}

/* ------------------------------------------------------------
   Widgets
   ------------------------------------------------------------ */

void ui_card(ui_rect r)
{
    /* Ombre douce : deux couches translucides decalees. */

    if (r.w > 4 && r.h > 4)
    {
        gfx_fill_rounded_rect_aa(U(r.x), U(r.y + 3), U(r.w), U(r.h), UI_RADIUS + 2, theme_shadow(), 10);
        gfx_fill_rounded_rect_aa(U(r.x), U(r.y + 1), U(r.w), U(r.h), UI_RADIUS + 1, theme_shadow(), 14);
    }

    ui_round_bordered(r, UI_RADIUS + 2, theme_panel(), theme_border());
}

/*
    Repeint l'interieur d'une carte avec sa couleur de fond OPAQUE connue.
    Sert aux repeints partiels (survol) : les widgets dessines ensuite ont
    des bords lisses, qui se melangent avec le fond ; partir d'un fond
    connu les rend identiques a chaque repeint (sinon les coins "durcissent"
    au fil des melanges successifs). Marge de 4 px : reste a l'interieur des
    coins arrondis de la carte.
*/

void ui_card_clear(ui_rect card)
{
    ui_rect in = ui_inset(card, 4);

    if (in.w > 0 && in.h > 0)
    {
        gfx_fill_rect(U(in.x), U(in.y), U(in.w), U(in.h), theme_panel());
    }
}

int ui_region(ui_ctx* c, u32 id, ui_rect r, int* hover, int* held)
{
    return ui_interact(c, id, r, hover, held);
}

void ui_separator(ui_rect r)
{
    if (r.w > 0)
    {
        gfx_fill_rect(U(r.x), U(r.y + r.h / 2), U(r.w), 1, ui_mix(theme_panel(), theme_border(), 70));
    }
}

void ui_label(ui_rect r, const char* text, int font, gfx_color color, ui_align align)
{
    s32 w = (s32)gfx_text_width(text, font);
    s32 x = r.x;

    if (align == UI_ALIGN_CENTER)      { x = r.x + (r.w - w) / 2; }
    else if (align == UI_ALIGN_RIGHT)  { x = r.x + r.w - w; }

    gfx_draw_text_ui_vcenter(U(x), U(r.y), U(r.h), text, color, font);
}

int ui_button(ui_ctx* c, u32 id, ui_rect r, const char* label, ui_button_style style)
{
    int hover, held;
    int clicked = ui_interact(c, id, r, &hover, &held);

    gfx_color base, text, edge;

    if (style == UI_BTN_PRIMARY)
    {
        base = theme_accent();
        text = theme_accent_text();
        edge = ui_mix(base, 0x000000, 18);
    }
    else if (style == UI_BTN_DANGER)
    {
        base = theme_close_bg();
        text = 0xFFFFFF;
        edge = ui_mix(base, 0x000000, 18);
    }
    else
    {
        base = theme_button_bg();
        text = theme_text();
        edge = theme_button_border();
    }

    /* Survol : eclaircit ; appui : assombrit. */

    if (held)       { base = ui_mix(base, 0x000000, 14); }
    else if (hover) { base = ui_mix(base, 0xFFFFFF, theme_is_dark() ? 10 : 18); }

    ui_round_bordered(r, UI_RADIUS, base, edge);

    s32 dy = held ? 1 : 0;   /* leger enfoncement du libelle */
    ui_rect t = { r.x, r.y + dy, r.w, r.h };

    ui_label(t, label, GFX_FONT_BODY, text, UI_ALIGN_CENTER);

    return clicked;
}

int ui_toggle(ui_ctx* c, u32 id, ui_rect r, int* value)
{
    int hover, held;
    int clicked = ui_interact(c, id, r, &hover, &held);

    if (clicked)
    {
        *value = !*value;
    }

    /* Piste en pilule + bouton rond. */

    gfx_color off = ui_mix(theme_panel(), theme_border(), 85);
    gfx_color track = *value ? theme_accent() : off;

    if (hover && !*value) { track = ui_mix(track, theme_text(), 8); }

    ui_round(r, (u32)(r.h / 2), track);

    s32 margin = 3;
    s32 knob = r.h - 2 * margin;
    s32 kx = *value ? r.x + r.w - margin - knob : r.x + margin;
    ui_rect k = { kx, r.y + margin, knob, knob };

    gfx_fill_rounded_rect_aa(U(k.x), U(k.y + 1), U(k.w), U(k.h), U(k.w / 2), 0x000000, 18);  /* ombre */
    ui_round(k, (u32)(k.w / 2), 0xFFFFFF);

    return clicked;
}

int ui_list_row(ui_ctx* c, u32 id, ui_rect r, const char* text, const char* detail, int selected)
{
    int hover, held;
    int clicked = ui_interact(c, id, r, &hover, &held);

    if (selected)
    {
        ui_round(r, UI_RADIUS - 2, theme_accent());
    }
    else if (hover)
    {
        ui_round(r, UI_RADIUS - 2, ui_mix(theme_panel(), theme_text(), held ? 12 : 7));
    }

    gfx_color fg = selected ? theme_accent_text() : theme_text();
    ui_rect inner = ui_inset(r, 0);

    inner.x += UI_PAD;
    inner.w -= 2 * UI_PAD;

    if (detail != (const char*)0)
    {
        ui_rect right = ui_cut_right(&inner, (s32)gfx_text_width(detail, GFX_FONT_SMALL));
        gfx_color dim = selected ? ui_mix(fg, theme_accent(), 25) : ui_mix(theme_text(), theme_panel(), 45);

        ui_label(right, detail, GFX_FONT_SMALL, dim, UI_ALIGN_RIGHT);
    }

    ui_label(inner, text, GFX_FONT_BODY, fg, UI_ALIGN_LEFT);

    return clicked;
}

int ui_text_field(ui_ctx* c, u32 id, ui_rect r, const char* text, const char* placeholder, int focused, int masked)
{
    int hover, held;
    int clicked = ui_interact(c, id, r, &hover, &held);

    gfx_color field = ui_mix(theme_panel(), theme_is_dark() ? 0xFFFFFF : 0x000000, theme_is_dark() ? 6 : 3);
    gfx_color edge = focused ? theme_accent() : (hover ? ui_mix(theme_border(), theme_text(), 25) : theme_border());

    ui_round(r, UI_RADIUS, edge);
    ui_round(ui_inset(r, focused ? 2 : 1), UI_RADIUS - 1, field);

    ui_rect inner = r;
    inner.x += UI_PAD;
    inner.w -= 2 * UI_PAD;

    char shown[64];
    u32 n = 0;

    while (text[n] != 0 && n < sizeof(shown) - 1)
    {
        shown[n] = masked ? '*' : text[n];
        n++;
    }

    shown[n] = 0;

    if (n == 0 && placeholder != (const char*)0)
    {
        ui_label(inner, placeholder, GFX_FONT_BODY, ui_mix(theme_text(), theme_panel(), 55), UI_ALIGN_LEFT);
    }
    else
    {
        ui_label(inner, shown, GFX_FONT_BODY, theme_text(), UI_ALIGN_LEFT);
    }

    if (focused)
    {
        /* Curseur : fine barre apres le texte. */

        s32 cx = inner.x + (s32)gfx_text_width(shown, GFX_FONT_BODY) + 1;
        s32 ch = (s32)gfx_font_height(GFX_FONT_BODY) - 6;

        gfx_fill_rect(U(cx), U(r.y + (r.h - ch) / 2), 2, U(ch), theme_accent());
    }

    return clicked;
}

void ui_progress(ui_rect r, u32 percent)
{
    if (percent > 100) { percent = 100; }

    ui_round(r, (u32)(r.h / 2), ui_mix(theme_panel(), theme_border(), 80));

    s32 fill = (r.w * (s32)percent) / 100;

    if (fill >= r.h)
    {
        ui_rect f = { r.x, r.y, fill, r.h };
        ui_round(f, (u32)(r.h / 2), theme_accent());
    }
}
