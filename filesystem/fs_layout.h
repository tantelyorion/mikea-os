#ifndef MIKEA_FS_LAYOUT_H
#define MIKEA_FS_LAYOUT_H


/*
====================================================

        Mikea OS Filesystem

        Disposition disque partagee (blocs reserves)

====================================================

    Correctif (persistance disque -- voir le README, section
    "Persistance disque", et le commentaire d'etat en tete de
    superblock.c) : jusqu'ici, seul le CONTENU des fichiers
    (filesystem/block.c, block_write()/block_read()) etait
    ecrit sur le vrai disque (build/disk.img) -- la table des
    inodes (quel nom, quelle taille, quel bloc de depart...) et
    le bitmap des blocs libres/utilises restaient dans deux
    tableaux en RAM, reinitialises a vide a CHAQUE demarrage
    (inode_init()/block_init()). Le contenu physique des blocs
    survivait donc bel et bien sur le disque, mais devenait
    orphelin -- plus aucune structure ne savait qu'il existait :
    du point de vue de l'utilisateur, tout etait perdu au
    redemarrage, comme documente dans le README.

    Ce fichier definit les quelques blocs de DONNEES (voir
    block.h, BLOCK_SIZE=512) reserves aux metadonnees du
    systeme de fichiers lui-meme plutot qu'a des donnees de
    fichiers, afin de pouvoir les relire au demarrage suivant
    (voir mkfs.c, inode_table_save()/load() dans inode.c,
    block_table_save()/load() dans block.c) :

        Bloc 0            : superbloc (deja reserve avant ce
                            correctif -- voir superblock.c,
                            superblock_write()/load(), et
                            block_allocate() ci-dessous qui
                            commence deja a 1, jamais 0)

        Blocs 1-23        : table des inodes (128 inodes *
                            92 octets = 11776 octets = tient
                            EXACTEMENT dans 23 blocs de 512
                            octets, sans octet perdu)

        Blocs 24-27       : bitmap des blocs (2048 octets =
                            TOTAL_BLOCKS, tient EXACTEMENT
                            dans 4 blocs)

    A partir du bloc 28 (FS_FIRST_DATA_BLOCK) : donnees de
    fichiers normales, ce que block_allocate() (block.c) peut
    desormais seul distribuer.

    Ces tailles sont figees par MAX_INODES (inode.h) et
    TOTAL_BLOCKS (block.h) : si l'un de ces deux nombres change
    un jour, les constantes ci-dessous DOIVENT etre recalculees
    en meme temps (sizeof(inode) * MAX_INODES / 512, arrondi au
    bloc superieur), sans quoi la table des inodes deborderait
    silencieusement sur le bitmap des blocs.
*/


#define FS_SUPERBLOCK_BLOCK 0


#define FS_INODE_TABLE_START_BLOCK 1

#define FS_INODE_TABLE_BLOCKS 23


#define FS_BLOCK_BITMAP_START_BLOCK 24

#define FS_BLOCK_BITMAP_BLOCKS 4


#define FS_FIRST_DATA_BLOCK 28


#endif
