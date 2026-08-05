#ifndef MIKEA_LIBC_STRING_H
#define MIKEA_LIBC_STRING_H


#include "../include/types.h"


/*
    Mini libc "freestanding" pour MikeaOS.

    Avant ce fichier, chaque module (user.c, process.c,
    input.c...) recopiait sa propre petite boucle pour
    mesurer/copier/comparer des chaines. Ce fichier centralise
    ces operations de base pour eviter la duplication et les
    bugs correspondants (ex. la comparaison tronquee corrigee
    dans security/user.c). Les modules existants n'ont pas ete
    modifies pour les utiliser automatiquement (pour ne rien
    casser), mais tout nouveau code peut s'appuyer dessus.
*/


u32 mk_strlen(const char* s);

int mk_strcmp(const char* a, const char* b);

int mk_strncmp(const char* a, const char* b, u32 n);

char* mk_strcpy(char* dest, const char* src, u32 max_len);

void* mk_memset(void* dest, int value, u32 len);

void* mk_memcpy(void* dest, const void* src, u32 len);

int mk_memcmp(const void* a, const void* b, u32 len);


#endif
