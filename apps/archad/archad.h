#ifndef MIKEA_ARCHAD_H
#define MIKEA_ARCHAD_H


/*
    ============================================================
    Archad -- archiveur natif MikeaOS (format OAR-lite)
    ============================================================

    Portage natif du logiciel "Archad" (fourni par l'utilisateur,
    projet Linux independant : voir son README pour l'original).
    L'original vise plusieurs formats (OAR maison, ZIP, RAR, TAR,
    GZ, 7Z) mais seul le format OAR maison y est reellement
    implemente -- les autres formats sont explicitement "simules"
    dans son propre code (voir archive.c, archad_open() :
    "Pour les autres formats... on simule juste l'ouverture").
    Ce portage se concentre donc, en toute coherence, sur CE
    format -- le seul reellement fonctionnel dans le projet
    d'origine, et le seul realiste sans les bibliotheques du
    monde Linux (zlib, etc.) totalement absentes de ce noyau
    independant.

    Difference assumee avec l'original : format d'en-tete plus
    compact (OAR_ENTRY plus bas, champs u32 au lieu de uint64_t,
    noms sur 32 caracteres au lieu de 256, voir FILE_NAME_SIZE)
    -- ce n'est pas une compatibilite binaire avec les .oar
    produits par le logiciel Linux d'origine, mais une
    reimplementation du meme PRINCIPE (en-tete + table d'entrees
    + donnees concatenees), adaptee aux conventions deja en place
    dans ce noyau (voir filesystem/inode.h).

    Limite structurelle de cette plateforme (voir
    filesystem/file.h, MAX_FILE_SIZE_BIN) : une archive OAR-lite
    est un fichier comme un autre, donc plafonnee a 64 Ko au
    total (en-tete + table d'entrees + toutes les donnees
    reunies) -- largement suffisant pour regrouper quelques
    petits fichiers, pas pour un vrai usage de sauvegarde/
    distribution de gros volumes.
*/


void cmd_archad_app();


#endif
