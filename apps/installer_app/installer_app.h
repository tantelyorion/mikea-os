#ifndef MIKEA_APP_INSTALLER_H
#define MIKEA_APP_INSTALLER_H


/*
    Installateur graphique : desormais une VRAIE fenetre du
    systeme de fenetres (voir gui/window.h) -- deplacable,
    reductible, fermable a la souris -- au lieu d'un habillage
    pose sur une console plein ecran. Appelle installer_run()
    (boot/installer/installer.h) directement, avec sa propre
    confirmation CLIQUABLE (boutons "Installer"/"Annuler")
    plutot que de taper "OUI" au clavier comme l'ancienne
    commande shell "install" (shell/commands.c, toujours
    disponible telle quelle depuis le Terminal).
*/

void cmd_installer_app();


#endif
