#ifndef MIKEA_VALIHA_H
#define MIKEA_VALIHA_H


/*
    ============================================================
    Valiha -- lecteur audio natif MikeaOS (WAV uniquement)
    ============================================================

    Portage natif du logiciel "Valiha" (fourni par l'utilisateur,
    deja presente comme "lecteur audio natif pour Mikea OS" dans
    son propre README -- mais construit en pratique sur une pile
    Linux de bureau complete : ncurses pour l'interface, mpg123
    pour le MP3, libsndfile pour le WAV/FLAC/OGG, libao pour la
    sortie son (voir son src/audio.c). Aucune de ces bibliotheques
    n'existe -- ni ne peut raisonnablement exister -- dans ce
    noyau independant, ecrit "from scratch" (voir le README
    principal du projet : aucun decodeur audio, seul le PCM brut
    est lu, voir kernel/drivers/soundblaster).

    Ce portage se limite donc, en toute franchise, au format WAV
    (RIFF/PCM) : le seul des formats vises par l'original qui ne
    demande AUCUN decodage (les octets audio sont deja les
    echantillons PCM, contrairement au MP3/FLAC/OGG qui exigent un
    veritable codec). Le MP3 -- le format "phare" de l'original --
    n'est donc pas supporte ici, comme les mp3 embarques du menu
    demarrage/extinction ont eux-memes du etre convertis en PCM
    brut hors ligne (voir kernel/drivers/soundblaster/sound_data.h)
    plutot que lus tels quels.

    Lecture via la carte Sound Blaster 16 (voir
    kernel/drivers/soundblaster/sb16.c) : le fichier WAV est
    converti a la volee (mono, 8 bits non signe -- le seul format
    gere par ce pilote) puis joue en un seul transfert DMA,
    BLOQUANT le temps de la lecture (meme principe que les sons
    systeme). Consequence assumee de la limite de taille de
    fichier de cette plateforme (voir filesystem/file.h,
    MAX_FILE_SIZE_BIN = 64 Ko) : quelques secondes de musique
    par piste tout au plus, pas un lecteur pour de vrais
    morceaux complets.
*/


void cmd_valiha_app();


#endif
