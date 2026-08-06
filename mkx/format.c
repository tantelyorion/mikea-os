#include "mkx.h"



void mkx_init()
{


/*

Initialisation du format MKX


Future:

- validation signature
- sécurité
- permissions


*/


/*
    Correctif (code non branche) : mkx_runtime_start()
    (mkx/runtime.c) existait mais n'etait ni declaree dans
    mkx.h ni appelee nulle part -- du code mort. On la relie
    ici, a l'initialisation du module mkx, comme les autres
    sous-systemes (mpm_init() relie de la meme facon
    installer_init()/database_init(), voir packages/mpm.c).
*/

mkx_runtime_start();


}