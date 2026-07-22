# assets/sounds

Aucun pilote audio n'existe encore dans MikeaOS (ni carte
son, ni PC speaker). Avant d'ajouter de vrais sons ou des
séquences de bips, il faut d'abord un driver minimal pilotant
le haut-parleur PC via le PIT (ports 0x61/0x43/0x42) ou une
carte son émulée (ex. Sound Blaster 16 sous QEMU).

À faire dans une prochaine étape si voulu :
1. `kernel/drivers/speaker/speaker.c` — bip via PC speaker.
2. Puis seulement des données ici (mélodies de démarrage,
   bips d'erreur, etc.).
