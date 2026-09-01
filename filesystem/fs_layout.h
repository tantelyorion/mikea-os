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

    Correctif (fichiers multi-blocs, voir inode.h,
    "indirect_block") : l'ajout de ce champ a fait passer
    sizeof(inode) de 92 a 96 octets -- 128 inodes * 96 = 12288
    octets = EXACTEMENT 24 blocs desormais (au lieu de 23),
    d'ou le decalage d'un bloc de tout ce qui suit.

        Bloc 0            : superbloc (deja reserve avant ce
                            correctif -- voir superblock.c,
                            superblock_write()/load(), et
                            block_allocate() ci-dessous qui
                            commence deja a 1, jamais 0)

        Blocs 1-24        : table des inodes (128 inodes *
                            96 octets = 12288 octets = tient
                            EXACTEMENT dans 24 blocs de 512
                            octets, sans octet perdu)

        Blocs 25-28       : bitmap des blocs (2048 octets =
                            TOTAL_BLOCKS, tient EXACTEMENT
                            dans 4 blocs)

    A partir du bloc 29 (FS_FIRST_DATA_BLOCK) : donnees de
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

#define FS_INODE_TABLE_BLOCKS 24


#define FS_BLOCK_BITMAP_START_BLOCK 25

#define FS_BLOCK_BITMAP_BLOCKS 4


#define FS_FIRST_DATA_BLOCK 29


#endif
