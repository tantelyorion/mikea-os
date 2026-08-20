#ifndef MIKEA_RTC_H
#define MIKEA_RTC_H


#include "../../../include/types.h"


/*
    ============================================================
    Mikea OS - Horloge temps reel (RTC/CMOS)
    ============================================================

    Avant ce fichier, ce projet n'avait aucune notion de date/
    heure "murale" -- seul timer_ticks() (kernel/drivers/timer)
    existait, qui ne compte que le temps ecoule depuis le
    demarrage (voir l'horloge de fonctionnement HH:MM:SS de
    gui/desktop.c). Ce module lit la puce RTC/CMOS standard de
    tout PC compatible (y compris emulee par QEMU/VirtualBox),
    sur les ports 0x70 (index) / 0x71 (donnee) -- la meme puce
    que celle geree par tout BIOS pour afficher l'heure au POST.

    Simplifications volontaires, documentees plutot que
    cachees :
    - Pas de gestion de fuseau horaire : l'heure renvoyee est
      celle telle que configuree dans la RTC (UTC par defaut
      sous QEMU, sauf option "-rtc base=localtime").
    - Pas de registre "siecle" (differe selon les BIOS, pas
      standardise) : les annees 00-99 lues sont supposees
      2000-2099, hypothese raisonnable pour un systeme actuel.
    - Aucune interruption RTC (IRQ8) : lecture a la demande
      uniquement (voir rtc_read(), utilisee par gui/desktop.c
      pour rafraichir l'horloge affichee, pas besoin d'un flux
      continu d'evenements pour un simple affichage HH:MM).
    ============================================================
*/


typedef struct
{

u32 year;   /* ex. 2026 */

u8 month;   /* 1-12 */

u8 day;     /* 1-31 */

u8 hour;    /* 0-23 */

u8 minute;  /* 0-59 */

u8 second;  /* 0-59 */

} rtc_time;


/*
    Lit l'heure courante dans "out". Utilise l'algorithme
    standard "lire deux fois, comparer" pour eviter de lire la
    RTC pendant sa propre mise a jour interne (registre Status A,
    bit "Update In Progress") -- une lecture en plein milieu de
    cette mise a jour renverrait des valeurs incoherentes (ex.
    "61 secondes").
*/

void rtc_read(rtc_time* out);


#endif
