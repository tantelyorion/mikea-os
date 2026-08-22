/*
====================================================

        Mikea OS Filesystem

        File Manager

        MKFS v2


        Developer:
        Tantely Orion


====================================================
*/


#ifndef MIKEA_FILE_H
#define MIKEA_FILE_H


#include "../include/types.h"



/*
====================================================
FILE CONFIGURATION
====================================================
*/


#define MAX_FILE_SIZE 512




/*
====================================================
FILE API
====================================================
*/


void file_init();



int file_create(
char* name
);


/*
    Variante de file_create() qui cree le fichier a l'interieur
    du dossier "parent" ("" pour la racine, comme
    directory_create()) au lieu de toujours creer a la racine.
    Utilisee par le bouton "Nouveau fichier" de l'explorateur
    graphique (apps/file_manager/file_manager.c) une fois a
    l'interieur d'un dossier. Memes regles de permission et de
    nom deja utilise/dans la corbeille que file_create().
*/

int file_create_in(
char* name,
char* parent
);



int file_write(
char* name,
char* data
);



char* file_read(
char* name
);



int file_delete(
char* name
);


/*
    ====================================================
    Corbeille
    ====================================================

    Avant ces fonctions, file_delete() ci-dessus etait la
    SEULE facon de supprimer un fichier -- suppression
    immediate et definitive (libere le bloc de donnees, voir
    inode_delete()), sans confirmation ni possibilite de
    recuperation, comme "rm" sous Unix. file_trash() offre une
    suppression "douce" : le fichier disparait des listages
    normaux mais son contenu reste recuperable jusqu'a
    file_restore() ou file_empty_trash() -- comportement
    attendu de tout bureau moderne (Windows/macOS/GNOME).
*/


/*
    Deplace "name" vers la corbeille (voir inode.h,
    champ "deleted") : le fichier disparait des listages
    normaux (file_exists() le considere absent) et de la
    lecture/ecriture normales, mais son contenu N'EST PAS
    efface -- recuperable par file_restore(). Renvoie 1 en cas
    de succes, 0 si le fichier n'existe pas (ou est deja dans
    la corbeille), ou si l'utilisateur courant n'a pas la
    permission d'ecriture (meme regle que file_delete()).
*/

int file_trash(
char* name
);


/*
    Sort "name" de la corbeille : redevient un fichier normal,
    visible et modifiable comme avant sa suppression. Renvoie 1
    en cas de succes, 0 si "name" n'existe pas ou n'est pas
    dans la corbeille.
*/

int file_restore(
char* name
);


/*
    Renvoie 1 si "name" est actuellement dans la corbeille, 0
    sinon (fichier absent ou fichier normal).
*/

int file_is_trashed(
char* name
);


/*
    Supprime DEFINITIVEMENT tout ce qui se trouve dans la
    corbeille (libere les blocs de donnees, comme
    file_delete()) -- irreversible, exactement comme "vider la
    corbeille" sur un bureau habituel. Renvoie le nombre de
    fichiers effectivement supprimes.
*/

u32 file_empty_trash();


/*
    Renomme "old_name" en "new_name" (mute l'inode en place --
    voir inode_find(), qui renvoie un pointeur direct vers
    l'entree reelle de la table, pas une copie). Renvoie 1 en
    cas de succes, 0 si "old_name" n'existe pas, si "new_name"
    est deja utilise (fichier, dossier, OU element de la
    corbeille -- meme contrainte de nom globalement unique que
    la creation, voir inode.h), ou sans permission d'ecriture.
*/

int file_rename(
char* old_name,
char* new_name
);



int file_exists(
char* name
);



#endif