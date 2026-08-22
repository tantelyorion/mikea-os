#ifndef MIKEA_POWER_H
#define MIKEA_POWER_H


/*
    ============================================================
    Mikea OS - Redemarrage / Extinction
    ============================================================

    Avant ce fichier, ce projet n'avait AUCUN moyen d'eteindre
    ou de redemarrer proprement -- fermer la fenetre QEMU etait
    la seule option, comme couper le courant sur une vraie
    machine. Ce module ajoute les deux fonctions de base
    attendues de tout systeme d'exploitation.

    Limitation assumee, documentee plutot que cachee : une
    extinction/redemarrage "propres" au sens plein (ACPI) exigent
    de lire et d'interpreter les tables ACPI (DSDT/FADT) fournies
    par le BIOS/UEFI -- un analyseur AML complet, largement hors
    de portee raisonnable pour ce projet. power_shutdown()
    utilise a la place les ports d'E/S "magiques" bien connus
    des principaux emulateurs (QEMU, Bochs, VirtualBox), documentes
    sur le wiki OSDev ("Shutdown") -- fonctionne dans ces
    emulateurs (donc dans QEMU, voir scripts/run.sh) mais PAS sur
    une vraie machine physique, ou seul power_reboot() (technique
    universelle, controleur clavier 8042) est garanti de
    fonctionner. Si aucune des methodes d'extinction ne
    fonctionne (materiel reel, ou emulateur non reconnu),
    power_shutdown() se rabat sur un simple message invitant a
    eteindre manuellement, plutot que de planter ou de boucler
    silencieusement.
    ============================================================
*/


/*
    Redemarre la machine (pulse la ligne de reset via le
    controleur clavier 8042, port 0x64 -- technique universelle,
    fonctionne aussi bien sur une vraie machine que sous
    n'importe quel emulateur). Ne revient jamais si elle
    reussit.
*/

void power_reboot();


/*
    Tente d'eteindre la machine via les ports "magiques" des
    principaux emulateurs (QEMU, Bochs, VirtualBox -- voir le
    commentaire ci-dessus). Ne revient jamais si l'un d'eux
    fonctionne ; sinon, affiche un message et rend la main
    (l'appelant doit alors inviter l'utilisateur a fermer la
    fenetre/eteindre manuellement).
*/

void power_shutdown();


#endif
