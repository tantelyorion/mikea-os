#ifndef MIKEA_SPEAKER_H
#define MIKEA_SPEAKER_H


#include "../../../include/types.h"


/*
    ============================================================
    Mikea OS - Haut-parleur interne (PC Speaker) + sons systeme
    ============================================================

    Ce projet n'a AUCUN pilote de carte son (ni AC97, ni HDA --
    en ecrire un est un chantier a part entiere, hors de portee
    ici). Le "haut-parleur interne" du PC (PC Speaker) est en
    revanche un composant universel de tout PC compatible x86
    (et QEMU l'emule par defaut) : un simple generateur de son
    carre, pilote via le canal 2 du PIT (meme puce que le minuteur
    systeme deja utilise par kernel/drivers/timer, mais un canal
    independant -- le reprogrammer ici ne perturbe pas l'horloge)
    et le port 0x61. Bien plus limite qu'une vraie carte son
    (un seul son a la fois, pas d'echantillons, juste des
    "bips"), mais c'est un son REEL, produit par une puce
    materielle existante -- pas une simulation.

    speaker_beep() est BLOQUANTE (la fonction ne revient qu'une
    fois le son termine) : les sons systeme ci-dessous sont tous
    courts (quelques centaines de millisecondes maximum), et
    bloquer brievement le thread appelant (ex. celui du bureau
    au demarrage, ou d'une fenetre de connexion) est sans
    consequence a cette echelle.
    ============================================================
*/


/*
    Joue un son de "frequency_hz" pendant "duration_ms"
    millisecondes, puis coupe le haut-parleur avant de rendre la
    main. "frequency_hz" a 0 coupe simplement le son (silence)
    pendant la duree indiquee -- utile pour separer deux notes
    d'une meme melodie.
*/

void speaker_beep(u32 frequency_hz, u32 duration_ms);


/*
    ============================================================
    Sons systeme nommes
    ============================================================

    Evenements REELLEMENT relies a ce jour :
    - sound_play_startup()  : voir kernel/kernel.c, fin du
      demarrage.
    - sound_play_shutdown() : voir kernel/drivers/power/power.c,
      power_shutdown() ET power_reboot().
    - sound_play_error()    : voir gui/login_screen.c (echec de
      connexion) et gui/desktop.c (ouverture d'application
      impossible, table de fenetres pleine).

    sound_play_usb_added()/sound_play_usb_removed() sont
    fournies ci-dessous (la melodie existe et fonctionne si on
    les appelle), mais NE SONT DECLENCHEES PAR AUCUN EVENEMENT
    REEL : ce projet n'a aucun pilote de controleur USB (ni
    UHCI/OHCI/EHCI/XHCI), donc aucune detection de branchement/
    debranchement n'existe pour les appeler automatiquement --
    en ecrire un est un chantier bien plus lourd que ce module
    de son. Elles restent prêtes a etre branchees le jour ou un
    pilote USB existera.
    ============================================================
*/

void sound_play_startup();

void sound_play_shutdown();

void sound_play_error();

void sound_play_usb_added();

void sound_play_usb_removed();


/*
    Coupe-circuit global (voir apps/settings/settings.c,
    bouton "Son") : quand desactive, TOUTES les fonctions
    sound_play_*() ci-dessus deviennent des no-op silencieux
    (speaker_beep() les ignore immediatement) -- pas persiste
    sur disque, remis a "active" par defaut a chaque
    redemarrage, meme principe que le theme clair/sombre
    (gui/theme.c).
*/

void sound_set_enabled(int enabled);

int sound_is_enabled();


#endif
