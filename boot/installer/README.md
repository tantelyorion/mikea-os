# boot/installer

Pas encore implemente.

Un installeur necessite au minimum :
1. Detection des disques disponibles (via BIOS INT 13h ou pilote AHCI).
2. Partitionnement (creation d'une table MBR ou GPT).
3. Formatage avec le systeme de fichiers maison (voir filesystem/mkfs.c).
4. Copie du bootloader (boot/bios) et du noyau sur le disque cible.

A construire une fois le pilote disque (filesystem/disk.c) valide sur
du materiel/une VM reelle, et une fois boot/loader capable de charger
depuis un vrai disque (pas seulement l'image ISO concatenee actuelle).
