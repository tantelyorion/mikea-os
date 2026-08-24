#include "theme.h"


/*
    Etat global unique (voir theme.h). Theme clair par defaut :
    c'est celui demande en priorite ("blanc et noir, gris") --
    le theme sombre reste un choix explicite (bouton "Mode
    sombre", voir apps/settings/settings.c), pas la valeur par
    defaut.
*/

static int dark_mode = 0;


/*
    Fond d'ecran courant (voir theme.h) -- meme principe qu'un
    seul etat global que "dark_mode" ci-dessus, pas persiste sur
    disque (comme le theme clair/sombre, remis a
    WALLPAPER_GRADIENT a chaque redemarrage).
*/

static wallpaper_style g_wallpaper_style = WALLPAPER_GRADIENT;


void theme_set_dark(int enabled)
{

dark_mode = enabled ? 1 : 0;

}


void theme_toggle_dark_mode()
{

dark_mode = !dark_mode;

}


int theme_is_dark()
{

return dark_mode;

}



/*
    Palette clair : gris tres clair pour le bureau (comme le
    fond de bureau gris de GNOME/Windows), panneaux blancs,
    bordures gris clair, texte quasi noir -- jamais de noir pur
    sur blanc pur (trop dur visuellement), ni de blanc pur sur
    noir pur en sombre, comme sur macOS/GNOME.
*/

#define LIGHT_DESKTOP_BG   0xE5E5EA
#define LIGHT_PANEL        0xFAFAFC
#define LIGHT_BORDER       0xD1D1D6
#define LIGHT_TITLEBAR     0xF5F5F7
#define LIGHT_TEXT         0x1C1C1E
#define LIGHT_BUTTON_BG    0xFFFFFF
#define LIGHT_BUTTON_BORDER 0xC7C7CC

#define DARK_DESKTOP_BG    0x151517
#define DARK_PANEL         0x242426
#define DARK_BORDER        0x3A3A3C
#define DARK_TITLEBAR      0x1E1E20
#define DARK_TEXT          0xF2F2F7
#define DARK_BUTTON_BG     0x2C2C2E
#define DARK_BUTTON_BORDER 0x48484A

/*
    Points d'accent neutres (gris), communs aux deux themes,
    SAUF le curseur : voir theme_cursor()/theme_cursor_outline()
    plus bas, qui s'inversent volontairement selon le theme
    (contrairement a tout le reste de cette palette).
*/

/*
    Rouge doux "traffic light" (bouton de fermeture) : le seul
    accent de couleur du theme, volontairement discret et
    reconnaissable (meme codage visuel que macOS/GNOME) plutot
    qu'un rouge vif "alerte".
*/

#define CLOSE_BG           0xE0564F
#define CLOSE_SYMBOL       0xFFFFFF


gfx_color theme_desktop_bg()
{

return dark_mode ? DARK_DESKTOP_BG : LIGHT_DESKTOP_BG;

}


gfx_color theme_panel()
{

return dark_mode ? DARK_PANEL : LIGHT_PANEL;

}


u32 theme_panel_opacity()
{

/*
    82% : assez opaque pour rester lisible, assez transparent
    pour que le fond existant transparaisse legerement --
    l'effet "verre depoli" recherche (voir gfx_fill_rect_blend()
    dans kernel/drivers/graphics/graphics.c).
*/

return 82;

}


gfx_color theme_border()
{

return dark_mode ? DARK_BORDER : LIGHT_BORDER;

}


gfx_color theme_titlebar_bg()
{

return dark_mode ? DARK_TITLEBAR : LIGHT_TITLEBAR;

}


gfx_color theme_text()
{

return dark_mode ? DARK_TEXT : LIGHT_TEXT;

}


gfx_color theme_titlebar_text()
{

return theme_text();

}


gfx_color theme_button_bg()
{

return dark_mode ? DARK_BUTTON_BG : LIGHT_BUTTON_BG;

}


gfx_color theme_button_border()
{

return dark_mode ? DARK_BUTTON_BORDER : LIGHT_BUTTON_BORDER;

}


gfx_color theme_close_bg()
{

return CLOSE_BG;

}


gfx_color theme_close_symbol()
{

return CLOSE_SYMBOL;

}


gfx_color theme_cursor()
{

/*
    Correctif (curseur invisible en theme clair) : ce curseur
    etait blanc quel que soit le theme, avec un simple contour
    noir fin (voir gui_draw_cursor(), gui/gui.c). Sur le theme
    clair, ou panneaux/boutons avoisinent deja le blanc pur
    (voir LIGHT_BUTTON_BG, LIGHT_PANEL ci-dessus), un curseur
    blanc s'y fond presque entierement -- seul le contour d'un
    pixel restait visible, bien trop fin pour suivre le
    curseur confortablement a l'oeil. Le curseur suit
    desormais le meme principe que ceux de
    Windows/macOS/GNOME : sombre sur fond generalement clair,
    clair sur fond generalement sombre. theme_cursor_outline()
    ci-dessous fait l'inverse, pour qu'un contour reste visible
    y compris sur les rares zones qui ont la couleur du
    remplissage (ex. survol d'un widget deja sombre en theme
    clair).
*/

return dark_mode ? 0xFFFFFF : 0x000000;

}


gfx_color theme_cursor_outline()
{

return dark_mode ? 0x000000 : 0xFFFFFF;

}


gfx_color theme_shadow()
{

return 0x000000;

}


u8 theme_text_attr()
{

/*
    Mode texte VGA (secours sans framebuffer graphique) : 0x0F =
    blanc sur noir (theme sombre), 0x70 = noir sur gris clair
    (theme clair). Meme esprit noir/blanc/gris que le mode
    graphique, avec la palette 16 couleurs fixe du VGA.
*/

return dark_mode ? 0x0F : 0x70;

}


void wallpaper_set_style(wallpaper_style style)
{

if (style >= 0 && style < WALLPAPER_STYLE_COUNT)
{

g_wallpaper_style = style;

}

}


wallpaper_style wallpaper_get_style()
{

return g_wallpaper_style;

}


const char* wallpaper_style_name(wallpaper_style style)
{

switch (style)
{

case WALLPAPER_GRADIENT: return "Degrade";

case WALLPAPER_STRIPES: return "Rayures";

case WALLPAPER_CHECKER: return "Damier";

case WALLPAPER_DOTS: return "Points";

case WALLPAPER_DIAGONAL: return "Diagonales";

case WALLPAPER_SOLID: return "Uni";

default: return "?";

}

}
