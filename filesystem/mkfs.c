#include "mkfs.h"

#include "disk.h"

#include "block.h"

#include "superblock.h"

#include "inode.h"

#include "file.h"

#include "directory.h"




void mkfs_init()
{

/*
    Ordre d'initialisation du systeme de fichiers :

    1. disk       (acces bas niveau au disque)
    2. block      (table des blocs libres/utilises)
    3. superblock (metadonnees globales du volume)
    4. inode      (table des inodes)
    5. file       (couche fichier)
    6. directory  (couche repertoire)

    Avant correction : seuls disk_init() et directory_init()
    etaient appeles, et file_system_init() (fonction inexistante)
    etait utilisee a la place de file_init(). Le systeme de
    fichiers ne demarrait donc jamais correctement.
*/


disk_init();


block_init();


superblock_init();


inode_init();


file_init();


directory_init();



}
