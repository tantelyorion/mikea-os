#include "mkfs.h"

#include "disk.h"

#include "block.h"

#include "superblock.h"

#include "inode.h"

#include "file.h"

#include "directory.h"


void console_write(const char* text);


/*
    Correctif (persistance disque) : distingue un disque de
    donnees DEJA FORMATE (redemarrage normal -- on doit RELIRE
    ce qui existe) d'un disque VIERGE (toute premiere execution,
    ou build/disk.img regenere a zero -- on doit FORMATER).

    On utilise le superbloc lui-meme comme marqueur : sur un
    disque.img cree par le Makefile (rempli de zeros), le
    magic "MIKEA" ne peut pas s'y trouver par hasard, donc si
    on le retrouve apres une lecture, c'est que ce disque a
    deja ete formate par une execution precedente.
*/

static int fs_already_formatted()
{

superblock* sb = superblock_get();

return (
sb->magic[0]=='M' &&
sb->magic[1]=='I' &&
sb->magic[2]=='K' &&
sb->magic[3]=='E' &&
sb->magic[4]=='A' &&
sb->version==2
);

}


void mkfs_init()
{

/*
    Ordre d'initialisation du systeme de fichiers :

    1. disk       (acces bas niveau au disque)
    2. superblock (relu en premier : c'est lui qui dit si le
                   disque est deja formate ou vierge, voir
                   fs_already_formatted() ci-dessus -- doit
                   donc passer AVANT block/inode desormais)
    3. block      (table des blocs libres/utilises)
    4. inode      (table des inodes)
    5. file       (couche fichier)
    6. directory  (couche repertoire)

    Avant correction : seuls disk_init() et directory_init()
    etaient appeles, et file_system_init() (fonction inexistante)
    etait utilisee a la place de file_init(). Le systeme de
    fichiers ne demarrait donc jamais correctement.

    Correctif (persistance disque, voir fs_layout.h) : block et
    inode ne sont plus systematiquement reinitialises a vide --
    sur un disque deja forme, on RECHARGE block_table et
    inode_table depuis le disque (block_table_load()/
    inode_table_load()) au lieu de les recreer vides. Voir le
    README, section "Persistance disque", pour l'etat
    precedent de ce correctif.
*/


disk_init();


superblock_load();


if (fs_already_formatted())
{

console_write(
"[fs] Disque de donnees deja forme -- chargement des "
"fichiers existants.\n"
);

block_table_load();

inode_table_load();

}
else
{

console_write(
"[fs] Disque de donnees vierge (ou non reconnu) -- "
"formatage initial.\n"
);

block_init();

superblock_init();

inode_init();

block_table_save();

inode_table_save();

}


file_init();


directory_init();



}
