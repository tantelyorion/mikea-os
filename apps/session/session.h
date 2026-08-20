#ifndef MIKEA_APP_SESSION_H
#define MIKEA_APP_SESSION_H

/*
    Session (application systeme par defaut "gui", mode
    graphique uniquement) : affiche l'utilisateur connecte,
    son role et ses permissions. Deplacee depuis
    shell/commands.c vers son propre fichier -- voir le
    commentaire equivalent dans apps/calculator/calculator.h.
*/

void cmd_gui();


/*
    Reutilise par apps/settings/settings.c (memes informations,
    presentees dans une fenetre differente avec un bouton de
    deconnexion en plus). "win_x"/"win_y" positionnent le
    contenu par rapport au coin superieur gauche de LA FENETRE
    appelante (pas des coordonnees absolues) -- necessaire
    depuis que les fenetres peuvent etre deplacees par
    glisser-deposer (voir gui_drag_update(), gui/gui.h) : sans
    cela, le contenu resterait fige a sa position d'origine
    pendant que le cadre de la fenetre, lui, suivrait la souris.
*/

void gui_draw_session_content(int win_x, int win_y);

#endif
