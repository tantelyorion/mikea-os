#include "settings.h"
#include "settings_view.h"
#include "../session/session.h"
#include "../../gui/gui.h"
#include "../../gui/theme.h"
#include "../../gui/desktop.h"
#include "../../gui/window.h"
#include "../../gui/ui.h"
#include "../../kernel/drivers/graphics/graphics.h"
#include "../../kernel/drivers/mouse/mouse.h"
#include "../../kernel/drivers/speaker/speaker.h"
#include "../../filesystem/block.h"
#include "../../kernel/memory/heap.h"
#include "../../kernel/drivers/rtc/rtc.h"
#include "../../security/user.h"
#include "../../shell/msh.h"

void console_write(const char* text);
void console_clear();
void keyboard_flush();
int keyboard_available();
char keyboard_getchar();
unsigned long timer_ticks();

#define GFX_SCALE 2

/* Disque, memoire et heure sont relus au plus toutes les 3 s (timer a 100 Hz). */

#define SETTINGS_SLOW_REFRESH_TICKS 300

/*
    ============================================================
    Reglages -- boucle d'evenements
    ============================================================

    La mise en page et le dessin vivent dans settings_view.c (widgets
    en pixels, voir gui/ui.h) ; ce fichier ne garde que ce qui touche
    au systeme : cadre de fenetre (barre de titre, deplacement,
    agrandissement, focus, dock) et application des actions.

    Deux niveaux de dessin :
    - redessin COMPLET (fond d'ecran, cadre, cartes) : au premier
      affichage, apres un deplacement, un changement de theme/fond...
    - repeint PARTIEL (contenu des cartes seulement) : simple changement
      de survol ou d'etat. Pas de fond d'ecran a recalculer, donc rapide
      et sans scintillement (voir ui_card_clear() pour l'idempotence).
    ============================================================
*/

/* Mesures lentes : espace disque, tas du noyau, date/heure. */

static void settings_refresh_slow(settings_model* m)
{
    u32 free_blocks = 0;

    for (u32 b = 0; b < TOTAL_BLOCKS; b++)
    {
        if (block_is_free(b))
        {
            free_blocks++;
        }
    }

    m->disk_free_kb = (free_blocks * BLOCK_SIZE) / 1024;
    m->disk_total_kb = (TOTAL_BLOCKS * BLOCK_SIZE) / 1024;

    u32 heap_used, heap_total;
    heap_stats(&heap_used, &heap_total);

    m->heap_used_kb = heap_used / 1024;
    m->heap_total_kb = heap_total / 1024;

    rtc_time now;
    rtc_read(&now);

    m->year = now.year;
    m->month = now.month;
    m->day = now.day;
    m->hour = now.hour;
    m->minute = now.minute;
}

/* Etat rapide : relu a chaque repeint pour refleter le systeme reel. */

static void settings_refresh_fast(settings_model* m)
{
    user* current = user_get_current();

    m->username = (current != (user*)0) ? current->username : (const char*)0;
    m->is_root = (current != (user*)0 && current->id == 1);
    m->dark = theme_is_dark();
    m->sound_on = sound_is_enabled();
    m->photo_index = wallpaper_get_photo_index();
    m->pattern_name = wallpaper_style_name(wallpaper_get_style());
}

void cmd_settings()
{
    if (!gfx_available())
    {
        console_write("Les parametres graphiques necessitent le mode graphique et une souris.\n");
        return;
    }

    int slot = gui_window_claim_slot();

    if (slot < 0)
    {
        return;
    }

    int win_x = 2, win_y = 2, win_w = 44, win_h = 27;

    /*
        Filet de securite : la fenetre ne depasse jamais l'ecran, meme a la
        plus petite resolution prise en charge (640x480, voir
        boot/loader/stage2.asm). En hauteur, elle ne doit pas non plus
        empieter sur la bande reservee a la barre des taches (voir
        gui_window_yield_click() plus bas).
    */

    {
        int max_w = (int)(gfx_width() / (8 * GFX_SCALE)) - 2 * win_x;
        int max_h = (int)(gfx_height() / (8 * GFX_SCALE)) - 2 - win_y;

        if (win_w > max_w) { win_w = max_w; }
        if (win_h > max_h) { win_h = max_h; }
    }

    int maximized = 0;
    int saved_x = win_x, saved_y = win_y, saved_w = win_w, saved_h = win_h;

    u32 close_bx = 0, close_by = 0, close_bsize = 0;
    u32 max_bx = 0, max_by = 0, max_bsize = 0;
    u32 min_bx = 0, min_by = 0, min_bsize = 0;

    settings_layout layout;
    settings_model model;
    settings_actions act;
    ui_ctx ui;

    ui.mouse_x = 0; ui.mouse_y = 0;
    ui.down = 0; ui.pressed = 0; ui.released = 0; ui.prev_down = 0;
    ui.hot = 0; ui.active = 0;

    /* Valeurs par defaut avant la premiere mesure (evite tout champ non initialise). */

    model.disk_free_kb = 0; model.disk_total_kb = 0;
    model.heap_used_kb = 0; model.heap_total_kb = 0;
    model.year = 0; model.month = 0; model.day = 0; model.hour = 0; model.minute = 0;

    int was_pressed = 0;
    int redraw = 1;
    int need_paint = 0;       /* repeint partiel demande */
    int suppress_edges = 0;   /* apres un redessin : ne pas transformer un bouton deja tenu en clic */
    int last_hover = 0;
    unsigned long last_slow = 0;

    gui_drag drag;
    drag.active = 0;

    while (1)
    {
        if (!gui_window_has_focus(slot))
        {
            redraw = 1;
            gui_window_idle();
            continue;
        }

        if (redraw)
        {
            settings_refresh_slow(&model);
            last_slow = timer_ticks();

            desktop_render_backdrop();

            gui_draw_window(
                win_x, win_y, win_w, win_h, "Réglages", theme_text_attr(),
                &close_bx, &close_by, &close_bsize,
                &max_bx, &max_by, &max_bsize, maximized,
                &min_bx, &min_by, &min_bsize
            );

            settings_view_layout(&layout, win_x, win_y, win_w, win_h);
            settings_view_paint_cards(&layout);

            was_pressed = mouse_left_pressed();
            redraw = 0;
            need_paint = 1;
            suppress_edges = 1;
        }

        s32 mx = mouse_get_x();
        s32 my = mouse_get_y();

        int now_pressed = mouse_left_pressed();
        int clicked = (now_pressed && !was_pressed);
        int released = (!now_pressed && was_pressed);

        was_pressed = now_pressed;

        /* Un clic dans la bande de la barre des taches appartient au dock, pas a cette fenetre. */

        if (clicked && (u32)my >= desktop_dock_top_px())
        {
            gui_cursor_erase();
            gui_window_yield_click(slot);
            continue;
        }

        if (!maximized && gui_drag_update(&drag, &win_x, &win_y, win_w, win_h, mx, my, now_pressed, close_bx, close_by, close_bsize))
        {
            redraw = 1;
        }

        if (clicked && gui_point_in_rect(close_bx, close_by, close_bsize, close_bsize, mx, my))
        {
            gui_cursor_erase();
            gui_window_close(slot);
            break;
        }
        else if (clicked && gui_point_in_rect(min_bx, min_by, min_bsize, min_bsize, mx, my))
        {
            gui_cursor_erase();
            gui_window_minimize(slot);
            continue;
        }
        else if (clicked && gui_point_in_rect(max_bx, max_by, max_bsize, max_bsize, mx, my))
        {
            if (!maximized)
            {
                saved_x = win_x; saved_y = win_y; saved_w = win_w; saved_h = win_h;
                win_x = 0;
                win_y = 0;
                win_w = (int)(gfx_width() / (8 * GFX_SCALE));
                win_h = (int)(gfx_height() / (8 * GFX_SCALE)) - 2;
                maximized = 1;
            }
            else
            {
                win_x = saved_x; win_y = saved_y; win_w = saved_w; win_h = saved_h;
                maximized = 0;
            }

            redraw = 1;
            was_pressed = 0;
        }

        /* ---- Contenu : repeint partiel si le survol, un clic ou l'etat a change ---- */

        if (!redraw)
        {
            int hover = drag.active ? 0 : settings_view_hit(&layout, mx, my);
            int stale = (timer_ticks() - last_slow) > SETTINGS_SLOW_REFRESH_TICKS;

            if (need_paint || hover != last_hover || clicked || released || stale)
            {
                int down = (drag.active || redraw) ? 0 : now_pressed;

                if (stale)
                {
                    settings_refresh_slow(&model);
                    last_slow = timer_ticks();
                }

                settings_refresh_fast(&model);

                /* Restaure le fond sous le curseur avant de repeindre (il est redessine en fin de tour). */

                gui_cursor_erase();

                if (suppress_edges)
                {
                    ui.prev_down = down;
                    suppress_edges = 0;
                }

                ui_begin(&ui, mx, my, down);
                settings_view_paint(&ui, &layout, &model, &act);

                last_hover = hover;
                need_paint = 0;

                if (act.logout)
                {
                    shell_request_logout();
                    gui_cursor_erase();
                    gui_window_close(slot);
                    break;
                }

                if (act.toggle_theme)
                {
                    theme_toggle_dark_mode();
                    redraw = 1;
                }

                if (act.toggle_sound)
                {
                    if (sound_is_enabled())
                    {
                        /* On joue le son de test AVANT de couper, pour entendre le reglage. */
                        sound_play_startup();
                        sound_set_enabled(0);
                    }
                    else
                    {
                        sound_set_enabled(1);
                        sound_play_startup();
                    }

                    need_paint = 1;
                }

                if (act.pattern)
                {
                    wallpaper_style next = (wallpaper_style)((wallpaper_get_style() + 1) % WALLPAPER_STYLE_COUNT);
                    wallpaper_set_style(next);
                    redraw = 1;
                }

                if (act.wallpaper >= 0)
                {
                    wallpaper_set_photo_index(act.wallpaper);
                    redraw = 1;
                }
            }
        }

        gui_draw_cursor(mx, my);

        if (keyboard_available())
        {
            char c = keyboard_getchar();

            if (c == '\n')
            {
                gui_cursor_erase();
                gui_window_close(slot);
                break;
            }
        }

        unsigned long frame_start = timer_ticks();

        while (timer_ticks() - frame_start < 1)
        {
        }
    }
}
