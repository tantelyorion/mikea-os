#ifndef MIKEA_APP_TERMINAL_H
#define MIKEA_APP_TERMINAL_H


/*
    Terminal : desormais une VRAIE fenetre du systeme de
    fenetres (voir gui/window.h) -- deplacable, reductible,
    agrandissable, fermable a la souris, comme les autres
    applications -- au lieu d'un simple habillage visuel pose
    par-dessus une console plein ecran (l'ancienne approche,
    dans gui/desktop.c). Fond noir a l'interieur de la fenetre,
    comme n'importe quel terminal reel.

    Reutilise execute_command() (shell/commands.c) via une
    redirection de sortie transparente (voir
    kernel/console/console.h, console_redirect_start()) : toutes
    les commandes shell existantes fonctionnent sans
    modification.
*/

void cmd_terminal_app();


#endif
