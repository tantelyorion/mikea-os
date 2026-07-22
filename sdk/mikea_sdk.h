#ifndef MIKEA_SDK_H
#define MIKEA_SDK_H


/*
    ============================================================
    Mikea OS - SDK applicatif
    ============================================================

    Ce fichier regroupe, en un seul endroit, les fonctions du
    noyau qu'une application (voir apps/) est autorisee et
    censee utiliser. Avant ce fichier, un developpeur d'app
    devait deviner quelles fonctions internes du noyau appeler
    et avec quel chemin d'include -- source d'erreurs et de
    couplage fragile avec les details internes du noyau.

    IMPORTANT - etat actuel du projet :
    Il n'existe PAS encore de separation memoire/privilege
    (pas de Ring 3, pas d'appel systeme via interruption ou
    syscall). Une "application" MikeaOS s'execute donc
    aujourd'hui dans le meme espace et au meme niveau de
    privilege que le noyau : ce header est une convention de
    programmation, pas encore une frontiere de securite reelle.
    Cette frontiere (via le format mkx + une vraie interruption
    syscall) est un travail futur.
    ============================================================
*/


#include "../include/types.h"



/* ---- Affichage (kernel/console) ---- */

void console_write(const char* text);


/* ---- Memoire (kernel/memory/heap) ---- */

void* mk_malloc(u32 size);

void mk_free(void* ptr);


/* ---- Chaines de caracteres (libc) ---- */

u32 mk_strlen(const char* s);

int mk_strcmp(const char* a, const char* b);


/* ---- Multitache cooperatif (kernel/process) ---- */

void thread_yield();


#endif
