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
    Correctif (fichiers multi-blocs) : taille maximale des
    fichiers BINAIRES (voir file_write_bin()/file_read_bin()
    plus bas) -- 128 blocs de 512 octets adressables via UN
    SEUL bloc indirect (voir inode.h, "indirect_block") : 512
    octets / 4 octets par numero de bloc = 128 entrees, donc
    128 * 512 = 65536 octets. Choisie pour correspondre
    exactement au tampon DMA de la carte son (voir
    kernel/drivers/soundblaster/sb16.c, DMA_BUFFER_SIZE) : un
    fichier audio a cette taille maximale peut etre joue en un
    seul transfert, sans avoir besoin d'un decoupage
    supplementaire cote lecteur (voir apps/valiha).
*/

#define MAX_FILE_SIZE_BIN 65536




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



/*
    ====================================================
    Fichiers BINAIRES (multi-blocs)
    ====================================================

    file_write()/file_read() ci-dessus restent des fonctions
    TEXTE : la donnee est une chaine C terminee par NUL (jamais
    de zero au milieu), limitee a 511 caracteres utiles (un
    seul bloc, voir MAX_FILE_SIZE). Inutilisable pour de
    l'audio ou une archive, qui contiennent forcement des
    octets zero et depassent largement 511 octets.

    file_write_bin()/file_read_bin() operent sur un tampon
    d'octets EXPLICITEMENT dimensionne (aucune fin de chaine
    supposee), jusqu'a MAX_FILE_SIZE_BIN (64 Ko), en chainant
    plusieurs blocs de donnees via un bloc indirect (voir
    inode.h, "indirect_block") quand la taille depasse 512
    octets -- transparent pour l'appelant.

    Les DEUX familles de fonctions partagent la meme table
    d'inodes (un fichier est un fichier, quel que soit la
    fonction utilisee pour le lire/l'ecrire) : un fichier cree
    par file_write_bin() peut etre supprime par file_delete(),
    liste par directory_list_children(), etc. -- seule la
    LECTURE/ECRITURE du contenu differe.
*/


/*
    Ecrit "length" octets de "data" (buffer brut, PAS une
    chaine -- peut contenir des zeros) dans le fichier "name"
    (cree si besoin, comme file_write()). Renvoie 1 en cas de
    succes, 0 si la permission d'ecriture manque, si le fichier
    est dans la corbeille, ou si "length" depasse
    MAX_FILE_SIZE_BIN, ou si le disque n'a plus assez de blocs
    libres pour stocker la totalite du fichier (dans ce dernier
    cas, tout bloc deja alloue pour cette tentative est libere
    avant de renvoyer 0 -- pas de fichier a moitie ecrit qui
    traine).
*/

int file_write_bin(
char* name,
const u8* data,
u32 length
);


/*
    Lit le fichier "name" dans "out_buffer" (deja alloue par
    l'appelant, taille "buffer_capacity"), quelle que soit la
    fonction ayant servi a l'ecrire (file_write() ou
    file_write_bin()). Renvoie le nombre d'octets reellement
    lus (0 si le fichier n'existe pas/est dans la corbeille, ou
    si "buffer_capacity" est trop petit pour la taille reelle
    du fichier -- jamais de lecture partielle silencieuse).
*/

u32 file_read_bin(
char* name,
u8* out_buffer,
u32 buffer_capacity
);



#endif