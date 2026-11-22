#ifndef MIKEA_SERIAL_H
#define MIKEA_SERIAL_H

#include "../../../include/types.h"

/*
    Port serie COM1 (16550). Sert de lien entre le noyau et un
    "cerveau" IA local tournant sur la machine hote (voir
    tools/jarvis_bridge.py). Le noyau n'execute JAMAIS ce qui
    revient par ce lien : il l'affiche seulement.
*/

void serial_init();
void serial_write(const char* text);
void serial_putc(char c);

/* Lit un octet, attend au plus "timeout_ticks" (1 tick = 10 ms). -1 si rien. */
int serial_getc_timeout(u32 timeout_ticks);

#endif
