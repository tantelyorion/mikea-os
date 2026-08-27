#ifndef MIKEA_SB16_H
#define MIKEA_SB16_H


#include "../../../include/types.h"


/*
    ============================================================
    Mikea OS - Carte son Sound Blaster 16 (DSP + DMA 8 bits)
    ============================================================

    Contrairement au haut-parleur interne (kernel/drivers/speaker,
    limite a des bips carres), la Sound Blaster 16 est une VRAIE
    carte son : elle sait lire un echantillon audio (PCM) et le
    restituer fidelement. QEMU l'emule (chipset "sb16"), tout
    comme la plupart des emulateurs x86 historiques -- mais AUCUN
    materiel recent (PC physique moderne) ne l'a plus reellement :
    ce driver est donc utile principalement sous QEMU (voir
    scripts/run.sh, options "-device sb16 -audiodev ...").

    Fonctionnement general (ports fixes 0x220-0x22F, IRQ5, DMA
    canal 1 -- reglages "historiques" par defaut, ceux que QEMU
    utilise sans configuration particuliere) :
    1. Reset + detection du DSP (sb16_init()).
    2. Pour chaque lecture : configuration du controleur DMA 8237
       (canal 1, transfert "read" memoire -> peripherique) sur un
       tampon PCM 8 bits non signe, mono, puis commande au DSP de
       lire ce tampon a la frequence d'echantillonnage voulue.

    Ce module reste volontairement simple (PAS d'IRQ, la fin de
    lecture est attendue par une boucle bloquante sur le minuteur
    systeme, meme principe que speaker_beep() -- voir
    kernel/drivers/speaker/speaker.c) : les sons systeme geres ici
    (demarrage/extinction) sont courts (quelques secondes maximum)
    et joues a des moments ou bloquer brievement le thread appelant
    est sans consequence.

    Limitation assumee du controleur DMA 8237 (8 bits, canaux 0-3) :
    le tampon transfere ne doit jamais chevaucher une frontiere de
    64 Ko en memoire physique. sb16_play_pcm() copie donc toujours
    les donnees a jouer (sound_data.c, potentiellement n'importe ou
    dans le noyau/.rodata) vers un tampon interne dedie, aligne sur
    64 Ko (voir sb16.c) -- plutot que d'exiger cet alignement des
    donnees sources elles-memes.
    ============================================================
*/


/*
    Tente de detecter une carte Sound Blaster 16 (reset + sequence
    de detection du DSP). A appeler une seule fois, tot au
    demarrage (voir kernel/kernel.c, section HARDWARE
    INITIALIZATION). Renvoie 1 si detectee et prete, 0 sinon (aucune
    carte son emulee/branchee -- cas normal sur du materiel reel ou
    un emulateur sans "-device sb16").
*/
int sb16_init();


/*
    Indique si une Sound Blaster 16 a ete detectee avec succes par
    sb16_init(). A verifier avant tout appel a sb16_play_pcm().
*/
int sb16_available();


/*
    Joue un echantillon PCM 8 bits NON SIGNE, MONO (le seul format
    gere ici -- voir sound_data.h) a la frequence "sample_rate_hz",
    de maniere BLOQUANTE (ne rend la main qu'une fois la lecture
    terminee, meme principe que speaker_beep()). "length" est la
    taille en octets de "data" ; DOIT tenir dans un seul transfert
    DMA 8 bits (65536 octets maximum, largement suffisant pour les
    sons systeme courts de ce projet -- voir sound_data.h).

    Ne fait rien si sb16_available() est faux, ou si
    sound_is_enabled() (kernel/drivers/speaker/speaker.h) est faux
    -- meme coupe-circuit global que les sons du PC Speaker.
*/
void sb16_play_pcm(const u8* data, u32 length, u32 sample_rate_hz);


#endif
