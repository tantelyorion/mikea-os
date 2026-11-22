#ifndef MIKEA_APP_SETTINGS_VIEW_H
#define MIKEA_APP_SETTINGS_VIEW_H

#include "../../gui/ui.h"

/*
    ============================================================
    Reglages - vue (mise en page + dessin en pixels)
    ============================================================

    Separee de la boucle d'evenements (settings.c) : cette partie ne
    depend que de gui/ui, du theme et du pilote graphique, donc elle
    se teste sur l'hote sans demarrer le noyau
    (tools/tests/settings_view_host_test.c).

    Disposition (fenetre de 640 a 800 px de large) :

        +---------------------------+------------------------+
        | Session (avatar, role)    | Fond d'ecran           |
        +---------------------------+  (6 vignettes + motif) |
        | Apparence (sombre, sons)  |                        |
        +---------------------------+------------------------+
        | Systeme : disque | memoire | date et heure         |
        +----------------------------------------------------+
        |  [Entree : fermer]                    [Deconnexion]|
        +----------------------------------------------------+
    ============================================================
*/

#define SETTINGS_SWATCH_COUNT 6

/* Donnees affichees : la vue ne lit rien elle-meme (pas de dependance noyau). */

typedef struct
{
    const char* username;      /* NULL = aucun utilisateur */
    int is_root;
    int dark;
    int sound_on;
    int photo_index;           /* fond photo actif */
    const char* pattern_name;  /* nom du motif procedural */
    u32 disk_free_kb, disk_total_kb;
    u32 heap_used_kb, heap_total_kb;
    u32 year, month, day, hour, minute;
} settings_model;

typedef struct
{
    ui_rect card_session, card_appearance, card_wallpaper, card_system, card_footer;
    ui_rect theme_toggle, sound_toggle;
    ui_rect swatch[SETTINGS_SWATCH_COUNT];
    ui_rect pattern_btn;
    ui_rect logout_btn;
    ui_rect hint;
} settings_layout;

/* Ce que l'utilisateur a declenche pendant un passage de dessin. */

typedef struct
{
    int toggle_theme;
    int toggle_sound;
    int wallpaper;       /* indice choisi, ou -1 */
    int pattern;
    int logout;
} settings_actions;

/* Identifiants de widgets (aussi renvoyes par settings_view_hit). */

#define SETTINGS_ID_THEME    1
#define SETTINGS_ID_SOUND    2
#define SETTINGS_ID_SWATCH0  3      /* 3 a 8 */
#define SETTINGS_ID_PATTERN  9
#define SETTINGS_ID_LOGOUT   10

/* Calcule la disposition pour une fenetre exprimee en CELLULES (comme gui_draw_window). */

void settings_view_layout(settings_layout* L, int win_x, int win_y, int win_w, int win_h);

/* Cartes avec ombre : seulement lors d'un redessin COMPLET de la fenetre. */

void settings_view_paint_cards(const settings_layout* L);

/*
    Repeint le contenu des cartes (repeint partiel sur de simples
    changements de survol : pas de fond d'ecran a redessiner, pas de
    scintillement). "ui" doit avoir recu ui_begin() juste avant.
*/

void settings_view_paint(ui_ctx* ui, const settings_layout* L, const settings_model* m, settings_actions* out);

/* Identifiant du widget sous (x, y), 0 si aucun. Sert a detecter un changement de survol. */

int settings_view_hit(const settings_layout* L, s32 x, s32 y);

#endif
