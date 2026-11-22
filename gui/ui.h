#ifndef MIKEA_GUI_UI_H
#define MIKEA_GUI_UI_H

#include "../include/types.h"
#include "../kernel/drivers/graphics/graphics.h"

/*
    ============================================================
    Mikea OS - Boite a outils de widgets (mise en page en PIXELS)
    ============================================================

    Remplace progressivement la grille de cellules de 16 px : un
    widget se place a n'importe quel pixel, avec des coins arrondis
    lisses, la police proportionnelle lissee et les couleurs du
    theme (clair/sombre automatique, voir gui/theme.h).

    Principe "mode immediat" : a CHAQUE image on appelle les widgets
    dans l'ordre de dessin ; ils dessinent ET retournent 1 si
    l'utilisateur vient de cliquer dessus. Pas de liste d'objets a
    gerer, pas d'allocation.

        ui_ctx ui;                               // une fois par fenetre
        ...
        ui_begin(&ui, mouse_get_x(), mouse_get_y(), mouse_left_pressed());
        ui_rect area = {20, 20, 300, 200};
        ui_rect row = ui_cut_top(&area, 32);
        if (ui_button(&ui, 1, row, "Valider", UI_BTN_PRIMARY)) { ... }

    "id" : entier non nul, unique dans la fenetre, identifie le widget
    d'une image a l'autre (survol / clic en cours).
    ============================================================
*/

typedef struct
{
    s32 x, y, w, h;
} ui_rect;

typedef struct
{
    s32 mouse_x, mouse_y;
    int down;        /* bouton gauche enfonce */
    int pressed;     /* vient d'etre enfonce (cette image) */
    int released;    /* vient d'etre relache (cette image) */
    int prev_down;
    u32 hot;         /* widget sous la souris */
    u32 active;      /* widget en cours de clic */
} ui_ctx;

/* Metriques communes (px). */

#define UI_RADIUS   8
#define UI_BTN_H    32
#define UI_ROW_H    36
#define UI_PAD      12

typedef enum { UI_BTN_NORMAL = 0, UI_BTN_PRIMARY = 1, UI_BTN_DANGER = 2 } ui_button_style;
typedef enum { UI_ALIGN_LEFT = 0, UI_ALIGN_CENTER = 1, UI_ALIGN_RIGHT = 2 } ui_align;

/* --- Mise en page ------------------------------------------------ */

ui_rect ui_inset(ui_rect r, s32 d);
ui_rect ui_cut_top(ui_rect* r, s32 h);      /* retire h px en haut de r et renvoie la tranche */
ui_rect ui_cut_bottom(ui_rect* r, s32 h);
ui_rect ui_cut_left(ui_rect* r, s32 w);
ui_rect ui_cut_right(ui_rect* r, s32 w);

/* --- Cycle de vie ------------------------------------------------ */

void ui_begin(ui_ctx* c, s32 mouse_x, s32 mouse_y, int left_down);

/* --- Widgets ----------------------------------------------------- */

void ui_card(ui_rect r);                                      /* panneau arrondi + ombre legere */
void ui_card_clear(ui_rect card);                             /* repeint l'INTERIEUR d'une carte (sans ombre ni bords) */

/*
    Zone cliquable sans dessin (vignettes, regions personnalisees) :
    meme logique de survol/clic que les boutons. Renvoie 1 au clic.
*/

int  ui_region(ui_ctx* c, u32 id, ui_rect r, int* hover, int* held);

void ui_separator(ui_rect r);                                 /* ligne fine au milieu de r */
void ui_label(ui_rect r, const char* text, int font, gfx_color color, ui_align align);

int  ui_button(ui_ctx* c, u32 id, ui_rect r, const char* label, ui_button_style style);
int  ui_toggle(ui_ctx* c, u32 id, ui_rect r, int* value);     /* 1 si la valeur a change */
int  ui_list_row(ui_ctx* c, u32 id, ui_rect r, const char* text, const char* detail, int selected);
int  ui_text_field(ui_ctx* c, u32 id, ui_rect r, const char* text, const char* placeholder, int focused, int masked);
void ui_progress(ui_rect r, u32 percent);

/* Melange deux couleurs : t_percent = part de "b" (0..100). */

gfx_color ui_mix(gfx_color a, gfx_color b, u32 t_percent);

#endif
