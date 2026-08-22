#ifndef MIKEA_APP_TRASH_H
#define MIKEA_APP_TRASH_H


/*
    Corbeille : application independante (voir gui/window.h),
    accessible depuis le Centre d'applications au meme titre que
    l'explorateur de fichiers -- auparavant un simple onglet
    cache a l'interieur de apps/file_manager/file_manager.c,
    desormais son propre programme, comme demande.

    Utilise l'API de corbeille de filesystem/file.c
    (file_trash()/file_restore()/file_is_trashed()/
    file_empty_trash()), partagee avec le bouton "Supprimer" de
    l'explorateur de fichiers -- les deux agissent sur les memes
    fichiers "supprimes", quelle que soit l'application qui les
    y a places.
*/

void cmd_trash_app();


#endif
