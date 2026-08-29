#ifndef MIKEA_OS_INSTALLER_H
#define MIKEA_OS_INSTALLER_H

#include "../../include/types.h"


/*
    Installeur MikeaOS (etape "installeur", voir le README a la
    racine du depot) : copie l'image de demarrage (secteur de
    boot + stage2 + noyau) du disque MAITRE (celui sur lequel le
    systeme a demarre) vers le disque ESCLAVE (celui utilise
    jusqu'ici uniquement comme disque de donnees, voir
    filesystem/disk.c), pour le rendre demarrable a son tour.

    ATTENTION : operation destructive. Le contenu actuel du
    disque esclave (systeme de fichiers de donnees) est
    entierement remplace par la copie de l'image de demarrage.
*/


/*
    Nombre de secteurs copies : 2048 secteurs = 1 Mo, la taille
    exacte a laquelle le Makefile complete l'image de demarrage
    (voir la cible $(IMG), "truncate -s 1M") -- copier ce nombre
    fixe de secteurs suffit donc toujours a copier l'image
    entiere, quelle que soit la taille reelle du noyau.
*/

#define INSTALLER_SECTOR_COUNT 2048


typedef enum
{

INSTALL_OK = 0,

INSTALL_ERROR_READ = 1,

INSTALL_ERROR_WRITE = 2,

INSTALL_ERROR_VERIFY = 3

} install_result;


/*
    Effectue l'installation. "progress_callback" est appele
    apres chaque secteur copie avec le numero de secteur en
    cours (0 a INSTALLER_SECTOR_COUNT-1), pour permettre
    d'afficher une progression -- peut etre nul si l'appelant
    ne veut pas de retour de progression. "failed_sector_out"
    (optionnel, peut etre nul) recoit le numero du secteur en
    cause en cas d'echec (INSTALL_OK laisse sa valeur
    inchangee) -- correctif (diagnostic d'echec invisible en
    mode graphique) : jusqu'ici, ce detail n'etait ecrit que
    via console_write(), qui dessine sur le curseur console
    GLOBAL (voir kernel/console/console.c), a un endroit de
    l'ecran sans rapport avec la fenetre de l'installateur --
    invisible ou immediatement recouvert par le bureau/la
    fenetre elle-meme. Le transmettre ainsi permet a l'appelant
    (apps/installer_app/installer_app.c) de l'afficher
    directement DANS sa propre fenetre.
*/

install_result installer_run(void (*progress_callback)(u32 sector, u32 total), u32* failed_sector_out);


#endif
