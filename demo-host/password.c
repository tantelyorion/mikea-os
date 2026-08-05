#include "password.h"



void password_init()
{

/*

Future:

- Algorithme cryptographique dedie (SHA-256, bcrypt...)
- Politique de complexite des mots de passe

*/

}



/*
    Hachage type FNV-1a (64 bits), sale.
    Deterministe, rapide, sans dependance libc :
    adapte a un noyau freestanding. A remplacer par un
    algorithme cryptographique reconnu des que le noyau
    aura le support arithmetique/materiel necessaire.
*/

u64 password_hash(const char* password, u32 salt)
{

u64 hash = 1469598103934665603ULL;

u64 prime = 1099511628211ULL;


hash ^= salt;

hash *= prime;


u32 i = 0;

while (password[i] != 0)
{

hash ^= (u8)password[i];

hash *= prime;

i++;

}


return hash;

}
