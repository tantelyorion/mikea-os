#include "power.h"

#include "../../cpu/io.h"


void console_write(const char* text);


void power_reboot()
{


console_write("Redemarrage...\n");


/*
    Attend que le tampon d'entree du controleur clavier 8042
    soit vide avant d'envoyer la commande -- sinon la
    commande de reset peut etre ignoree si le controleur est
    deja occupe (meme prudence que les autres ecritures vers
    ce controleur, voir kernel/drivers/keyboard/keyboard.c).
    Bit 1 du port de statut (0x64) = "tampon d'entree plein".
*/

u32 timeout = 100000;

while ((inb(0x64) & 0x02) != 0 && timeout > 0)
{

timeout--;

}


/*
    0xFE = pulse la ligne de reset du controleur clavier 8042
    -- technique universelle documentee sur le wiki OSDev
    ("Reboot"), fonctionne aussi bien sur une vraie machine
    que sous n'importe quel emulateur.
*/

outb(0x64, 0xFE);


/*
    Si on arrive ici, le reset n'a (tres rarement) pas
    fonctionne : on bloque plutot que de continuer a executer
    du code dans un etat indetermine.
*/

while (1)
{

asm volatile("hlt");

}


}


void power_shutdown()
{


console_write("Extinction...\n");


/*
    QEMU (chipset ACPI PIIX4/Q35 par defaut) : ecrire 0x2000
    dans le registre PM1a_CNT, mappe sur le port 0x604 par
    defaut. C'est la methode qui s'applique a ce projet (voir
    scripts/run.sh, aucune option "-M" particuliere n'est
    passee a QEMU).
*/

outw(0x604, 0x2000);


/* Bochs et anciennes versions de QEMU. */

outw(0xB004, 0x2000);


/* VirtualBox. */

outw(0x4004, 0x3400);


/*
    Aucune des methodes ci-dessus n'a fonctionne (materiel
    reel, ou emulateur non reconnu -- voir le commentaire de
    power.h) : on ne peut pas eteindre la machine nous-memes
    sans un veritable analyseur ACPI. On previent
    honnetement plutot que de pretendre avoir reussi.
*/

console_write("Extinction automatique non disponible sur cette machine.\n");

console_write("Vous pouvez fermer la fenetre ou eteindre manuellement.\n");


}
