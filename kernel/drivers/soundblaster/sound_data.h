#ifndef MIKEA_SOUND_DATA_H
#define MIKEA_SOUND_DATA_H


#include "../../../include/types.h"


/*
    ============================================================
    Mikea OS - Sons systeme embarques (donnees brutes PCM)
    ============================================================

    Meme principe que gui/assets/wallpaper_images.h (photos
    fournies par l'utilisateur, converties une seule fois, hors
    ligne, en donnees brutes) : ce noyau n'a et n'aura aucun
    decodeur MP3 (bien plus complexe qu'un decodeur JPEG/PNG,
    hors de portee ici). "startup.mp3" et "shutdown.mp3"
    (fournis par l'utilisateur) ont ete convertis UNE SEULE FOIS,
    hors ligne, avec ffmpeg :

        ffmpeg -i startup.mp3  -f u8 -ar 11025 -ac 1 startup.pcm
        ffmpeg -i shutdown.mp3 -f u8 -ar 11025 -ac 1 shutdown.pcm

    puis le resultat brut (PCM 8 bits NON SIGNE, mono, 11025 Hz --
    le format natif attendu par le DSP Sound Blaster 16 pour une
    lecture 8 bits, voir soundblaster/sb16.c) a ete compile
    directement dans sound_data.c (genere automatiquement par
    tools/gen_sound_data.py -- ne jamais modifier ce fichier a la
    main).

    Compromis assume, dicte par la place reservee au noyau sur le
    disque de demarrage (voir boot/loader/stage2.asm, dap_kernel) :
    11025 Hz / 8 bits est nettement en dessous de la qualite des
    fichiers mp3 d'origine (probablement 44.1 kHz stereo), mais
    reste un son REEL et reconnaissable pour une melodie courte de
    demarrage/extinction -- pas de simple bip synthetique (voir
    kernel/drivers/speaker, qui reste utilise en repli si aucune
    carte Sound Blaster 16 n'est detectee, voir sb16.c).
    ============================================================
*/


extern const u8 startup_pcm[];
extern const u32 startup_pcm_len;

extern const u8 shutdown_pcm[];
extern const u32 shutdown_pcm_len;


/* Frequence d'echantillonnage commune aux deux clips (voir la commande ffmpeg ci-dessus). */
#define SOUND_PCM_SAMPLE_RATE 11025


#endif
