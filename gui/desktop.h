#ifndef MIKEA_GUI_DESKTOP_H
#define MIKEA_GUI_DESKTOP_H


#include "../include/types.h"

#include "theme.h"


/*
    ============================================================
    Mikea OS - Bureau graphique
    ============================================================

    Avant ce fichier, la seule facon d'atteindre une application
    graphique (calculatrice, explorateur de fichiers...) etait de
    taper sa commande texte ("calc", "files"...) a l'invite du
    shell -- rien ne ressemblait a un vrai bureau (pas de fond
    d'ecran, pas de barre des taches, pas de lancement au clic).
    Ce module ajoute cette couche : un bureau plein ecran avec
    barre des taches (icones d'applications + horloge de
    fonctionnement + bouton de deconnexion), dans l'esprit
    Windows/GNOME/macOS, affiche AUTOMATIQUEMENT apres la
    connexion des qu'un mode graphique est disponible (voir
    shell/msh.c) -- sans qu'aucune commande ne soit necessaire.

    En l'absence de mode graphique (gfx_available() faux, ex.
    VBE non detecte au demarrage), ce module n'est pas utilise :
    shell/msh.c retombe alors sur l'invite texte habituelle,
    seule option viable sans framebuffer pixel.
    ============================================================
*/


/*
    Affiche le bureau graphique et bloque jusqu'a ce que
    l'utilisateur demande explicitement a se deconnecter
    (bouton "Deconnexion" de la barre des taches, ou commande
    "logout" tapee dans le terminal integre). Ne deconnecte pas
    l'utilisateur elle-meme (voir security/user.h,
    user_logout()) : c'est a l'appelant (shell/msh.c) de le
    faire en reponse a cette fin d'appel, exactement comme pour
    l'ancienne boucle texte.
*/

void gui_desktop_run();


/*
    Dessine le fond du bureau (degrade + barre des taches, sans
    le Centre d'applications) sans bloquer -- a appeler par les
    applications (voir apps/calculator/calculator.c et les
    autres) au lieu d'un simple console_clear(), pour que le
    bureau reste visible derriere leur fenetre plutot que de
    disparaitre completement. Sans effet si gfx_available() est
    faux (mode texte).
*/

void desktop_render_backdrop();


/*
    Coordonnee Y (en pixels) du bord SUPERIEUR du Dock (voir
    gui/desktop.c, desktop_draw_taskbar()) -- utilisee par
    chaque application pour savoir si un clic doit lui etre
    transmis normalement ou etre cede au bureau (voir
    gui_window_yield_click(), gui/window.c), quand ce clic
    tombe dans la zone du Dock plutot que dans le contenu de
    la fenetre elle-meme.

    Correctif (bureau macOS) : chaque application calculait
    auparavant elle-meme cette limite ("rows_now - 2" lignes de
    texte), en supposant l'ANCIENNE barre des taches (pleine
    largeur, ancree tout en bas, exactement 2 lignes de haut).
    Le nouveau Dock etant un element flottant de hauteur FIXE
    independante du nombre de fenetres ouvertes (voir
    DOCK_PADDING/DOCK_ICON_SIZE/DOCK_BOTTOM_MARGIN, gui/desktop.c),
    sa position ne varie qu'avec la hauteur d'ecran -- centraliser
    ce calcul ICI garantit que les sept applications qui en
    dependent (calculatrice, gestionnaire de fichiers,
    installateur, session/compte, parametres, terminal, corbeille)
    restent TOUJOURS coherentes avec le Dock reellement dessine,
    au lieu de chacune recalculer (et desormais mal recalculer)
    la meme chose separement.
*/

u32 desktop_dock_top_px();


/*
    Dessine un motif de fond d'ecran (voir gui/theme.h,
    wallpaper_style) dans le rectangle pixel ("x","y","w","h")
    donne -- pas forcement plein ecran : utilisee par
    desktop_render_backdrop() pour tout l'ecran, et par
    apps/settings/settings.c pour les vignettes miniatures du
    selecteur de fond d'ecran (memes motifs, juste plus petits).
*/

void gui_paint_wallpaper_area(u32 x, u32 y, u32 w, u32 h, wallpaper_style style);


/*
    Meme principe que gui_paint_wallpaper_area() ci-dessus, mais
    pour une VRAIE photo (voir gui/assets/wallpaper_images.h),
    reperee par son indice dans WALLPAPER_PHOTOS[] plutot que par
    un style procédural. Affichee "en mosaique" (voir le
    commentaire dans gui/desktop.c) : chaque pixel de la photo
    (80x60) devient un bloc agrandi pour remplir le rectangle
    cible. Sans effet si "photo_index" est hors bornes.
*/

void gui_paint_photo_area(u32 x, u32 y, u32 w, u32 h, u32 photo_index);


#endif
