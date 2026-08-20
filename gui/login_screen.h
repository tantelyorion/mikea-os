#ifndef MIKEA_GUI_LOGIN_SCREEN_H
#define MIKEA_GUI_LOGIN_SCREEN_H


#include "../security/user.h"


/*
    ============================================================
    Mikea OS - Ecran de connexion graphique
    ============================================================

    Remplace l'invite texte "Mikea OS login:" / "Password:"
    (security/login.c, mode graphique uniquement -- le texte
    reste utilise en secours si aucun mode graphique n'est
    disponible) par un vrai panneau clic-et-tape : champs
    "Utilisateur"/"Mot de passe" cliquables (voir
    gui_draw_field(), gui/gui.c), bouton "Se connecter", message
    d'erreur en cas d'identifiants invalides -- dans le meme
    theme "verre depoli" que le reste du bureau (gui/theme.c),
    au lieu d'un texte de console brut.
    ============================================================
*/


/*
    Bloque jusqu'a une connexion reussie (ne renvoie jamais 0) :
    memes garanties que login_prompt() (security/login.c), dont
    c'est l'equivalent graphique. A n'appeler que si
    gfx_available() est vrai.
*/

user* gui_login_screen();


#endif
