/*
====================================================

        Mikea OS Filesystem

        File Manager


        MKFS v2


====================================================
*/


#include "file.h"


#include "inode.h"

#include "block.h"

#include "../security/user.h"

#include "../security/permission.h"






/*
====================================================
TEMP BUFFER

Cache lecture

====================================================
*/


static char read_buffer[MAX_FILE_SIZE];



/*
    Avant l'appel a user_login() (ex. pendant
    l'initialisation du systeme de fichiers au demarrage),
    aucun utilisateur n'est encore connecte : on autorise
    dans ce cas precis (contexte systeme), sinon on applique
    la table de permissions reelle de l'utilisateur connecte.
*/

static int current_user_can(int permission)
{

user* u = user_get_current();


if (u == 0)
{

return 1;

}


return check_permission((int)u->id, permission);

}






/*
====================================================
INITIALIZE FILE SYSTEM
====================================================
*/


void file_init()
{


for(
int i=0;
i<MAX_FILE_SIZE;
i++
)
{


read_buffer[i]=0;


}



}






/*
====================================================
CREATE FILE
====================================================
*/


int file_create(
char* name
)
{

return file_create_in(name, "");

}



int file_create_in(
char* name,
char* parent
)
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


inode* node;



node =
inode_create_ex(name, parent, 0);



if(node==0)
{

return 0;

}


/*
    Correctif (nom "invisible" mais indisponible) : sans ce
    controle, creer un fichier portant le meme nom qu'un
    element actuellement dans la corbeille (voir file_trash()
    plus bas) reussirait silencieusement -- inode_create()
    trouve l'entree existante et la renvoie telle quelle,
    encore marquee "deleted", donc toujours absente des
    listages malgre le succes apparent de la creation. On
    bloque ce cas explicitement : il faut restaurer ou vider la
    corbeille avant de pouvoir reutiliser ce nom.
*/

if(node->deleted)
{

return 0;

}



return 1;


}







/*
====================================================
WRITE FILE

Data -> Block

====================================================
*/


int file_write(
char* name,
char* data
)
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


inode* node;



node =
inode_find(name);



if(node==0)
{


node =
inode_create(name);



}



if(node==0)
{

return 0;

}


/*
    Meme regle que file_create() ci-dessus : un nom dans la
    corbeille n'est pas disponible en ecriture non plus, tant
    qu'il n'a pas ete restaure ou que la corbeille n'a pas ete
    videe.
*/

if(node->deleted)
{

return 0;

}




u8 buffer[512];



for(
int i=0;
i<512;
i++
)
{


buffer[i]=0;


}






int i=0;



while(
data[i]
&&
i<511
)
{


buffer[i]=data[i];


i++;


}



buffer[i]=0;




/*
Write block

*/

block_write(
node->block,
buffer
);



node->size=i;


/*
    Correctif (persistance disque -- decouvert en testant les
    fichiers multi-blocs, voir file_write_bin() plus bas, mais
    touche EXACTEMENT autant cette fonction, presente depuis
    l'origine) : le CONTENU du fichier est bien ecrit sur le
    vrai disque (block_write() ci-dessus), mais la TAILLE mise
    a jour sur l'inode ne l'etait que dans le tableau en RAM --
    jamais persistee elle-meme, sauf par coincidence si une
    AUTRE creation/suppression d'inode declenchait entre-temps
    une sauvegarde complete de la table (voir
    inode_table_save(), appelee seulement par
    inode_create_ex()/inode_delete() avant ce correctif). Sans
    cette sauvegarde explicite ici, un fichier ecrit puis
    JAMAIS suivi d'une autre creation/suppression avant le
    redemarrage se retrouvait avec une taille remise a 0 au
    rechargement -- son contenu restait bien present sur le
    disque, mais devenait illisible (file_read() n'aurait rien
    a lire).
*/

inode_table_save();


return 1;


}







/*
====================================================
READ FILE

Block -> Memory

====================================================
*/


char* file_read(
char* name
)
{


/*
    Correctif : file_create()/file_write()/file_delete()
    exigent tous les trois PERMISSION_WRITE via
    current_user_can(), mais file_read() ne verifiait rien du
    tout -- n'importe quel utilisateur connecte, meme sans la
    moindre permission accordee (voir security/permission.c),
    pouvait lire le contenu de n'importe quel fichier avec
    "cat". On applique ici la meme regle en lecture.
*/

if(!current_user_can(PERMISSION_READ))
{

return 0;

}


inode* node;



node =
inode_find(name);



if(node==0)
{

return 0;

}


/*
    Meme regle que file_create()/file_write() ci-dessus : un
    fichier dans la corbeille n'est pas lisible normalement non
    plus (voir gui/file_manager pour la vue "Corbeille" dediee,
    qui ne montre que les noms, pas le contenu).
*/

if(node->deleted)
{

return 0;

}




u8 buffer[512];



block_read(
node->block,
buffer
);




for(
int i=0;
i<512;
i++
)
{


read_buffer[i]=buffer[i];


}



return read_buffer;


}








/*
====================================================
DELETE FILE
====================================================
*/


int file_delete(
char* name
)
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


inode* node;



node =
inode_find(name);



if(node==0)
{

return 0;

}



inode_delete(name);



return 1;


}








/*
====================================================
CHECK FILE EXISTENCE
====================================================
*/


int file_exists(
char* name
)
{


inode* node;



node =
inode_find(name);



if(node && !node->deleted)
{

return 1;

}



return 0;


}



/*
====================================================
CORBEILLE
====================================================
*/


int file_trash(
char* name
)
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


inode* node;



node =
inode_find(name);



if(node==0)
{

return 0;

}


if(node->deleted)
{

return 0;

}


node->deleted = 1;


return 1;


}



int file_restore(
char* name
)
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


inode* node;



node =
inode_find(name);



if(node==0)
{

return 0;

}


if(!node->deleted)
{

return 0;

}


node->deleted = 0;


return 1;


}



int file_is_trashed(
char* name
)
{


inode* node;



node =
inode_find(name);



if(node==0)
{

return 0;

}


return node->deleted;


}



u32 file_empty_trash()
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


u32 removed = 0;


/*
    Parcourt TOUS les inodes existants (utilises ou non --
    inode_get() gere l'index hors bornes en renvoyant 0, voir
    inode.c) plutot qu'une liste separee des elements de la
    corbeille : ce systeme de fichiers ne modelise les fichiers
    que comme une liste plate d'inodes (voir la note dans
    filesystem/directory.c), donc pas de structure de repertoire
    "corbeille" dediee a maintenir en plus.
*/

for(u32 i = 1; i <= MAX_INODES; i++)
{


inode* node = inode_get(i);


if(node != 0 && node->used && node->deleted)
{


char name_copy[FILE_NAME_SIZE];

int j = 0;

while(node->name[j] && j < FILE_NAME_SIZE - 1)
{

name_copy[j] = node->name[j];

j++;

}

name_copy[j] = 0;


inode_delete(name_copy);

removed++;


}


}


return removed;


}



int file_rename(
char* old_name,
char* new_name
)
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


if(new_name == 0 || new_name[0] == 0)
{

return 0;

}


inode* node = inode_find(old_name);


if(node == 0)
{

return 0;

}


if(inode_find(new_name) != 0)
{

/* Nom deja utilise (fichier, dossier, ou element de la corbeille). */

return 0;

}


u32 j = 0;

while(new_name[j] && j < FILE_NAME_SIZE - 1)
{

node->name[j] = new_name[j];

j++;

}

node->name[j] = 0;


return 1;


}


/*
====================================================
FICHIERS BINAIRES (multi-blocs)

Voir file.h pour la description generale.
====================================================
*/


void* mk_memcpy(void* dest, const void* src, u32 len);


int file_write_bin(
char* name,
const u8* data,
u32 length
)
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


if(length > MAX_FILE_SIZE_BIN)
{

return 0;

}


inode* node;

node = inode_find(name);

if(node==0)
{

node = inode_create(name);

}

if(node==0)
{

return 0;

}

if(node->deleted)
{

return 0;

}


/*
    Chemin COURT (inchange) : la donnee tient dans le seul
    bloc direct deja alloue a la creation de l'inode (voir
    inode_create_ex(), filesystem/inode.c) -- aucun bloc
    indirect necessaire. Libere d'abord un eventuel ANCIEN
    bloc indirect si ce fichier en avait un (cas d'un fichier
    precedemment plus gros, reecrit plus petit).
*/

if(length <= BLOCK_SIZE)
{


if(node->indirect_block != 0)
{

u8 old_indirect[BLOCK_SIZE];

block_read(node->indirect_block, old_indirect);

u32* old_list = (u32*)old_indirect;

u32 max_entries = BLOCK_SIZE / sizeof(u32);

for(u32 i=0; i<max_entries; i++)
{

if(old_list[i] != 0) { block_free(old_list[i]); }

}

block_free(node->indirect_block);

node->indirect_block = 0;

}


u8 buffer[BLOCK_SIZE];

for(u32 i=0; i<BLOCK_SIZE; i++) { buffer[i] = 0; }

mk_memcpy(buffer, data, length);


block_write(node->block, buffer);


node->size = length;


/* Correctif (persistance disque) : voir le commentaire de file_write() plus haut -- meme raisonnement exactement. */

inode_table_save();


return 1;

}


/*
    Chemin LONG : plus d'un bloc necessaire. Calcule combien de
    blocs de donnees il faut (le premier reste "node->block",
    deja alloue -- reutilise comme PREMIER bloc de donnees plutot
    que de le laisser inutilise), alloue les blocs
    supplementaires ainsi qu'un bloc indirect pour les
    reference tous.

    Correctif (pas de fichier a moitie ecrit) : si l'allocation
    echoue en cours de route (disque plein), TOUS les blocs
    deja alloues pour cette tentative sont liberes avant de
    renvoyer 0 -- l'ancien contenu du fichier (s'il y en avait
    un) reste malheureusement deja efface a ce stade (meme
    limite que file_write() existant, qui n'a jamais ete
    transactionnel non plus).
*/

u32 blocks_needed = (length + BLOCK_SIZE - 1) / BLOCK_SIZE;

u32 max_entries = BLOCK_SIZE / sizeof(u32);


if(blocks_needed > max_entries)
{

/* Ne peut pas arriver si length <= MAX_FILE_SIZE_BIN, mais garde-fou explicite. */

return 0;

}


/* Libere l'ancien bloc indirect (et sa chaine) si ce fichier en avait deja un -- on va en ecrire un nouveau de toute facon. */

if(node->indirect_block != 0)
{

u8 old_indirect[BLOCK_SIZE];

block_read(node->indirect_block, old_indirect);

u32* old_list = (u32*)old_indirect;

for(u32 i=0; i<max_entries; i++)
{

if(old_list[i] != 0) { block_free(old_list[i]); }

}

block_free(node->indirect_block);

node->indirect_block = 0;

}


u32 block_list[128];

for(u32 i=0; i<max_entries; i++) { block_list[i] = 0; }


/* Premier bloc : reutilise node->block (deja alloue depuis la creation de l'inode). */

block_list[0] = node->block;


int alloc_failed = 0;

for(u32 i=1; i<blocks_needed; i++)
{

u32 b = block_allocate();

if(b == 0)
{

alloc_failed = 1;

break;

}

block_list[i] = b;

}


u32 new_indirect = 0;

if(!alloc_failed)
{

new_indirect = block_allocate();

if(new_indirect == 0)
{

alloc_failed = 1;

}

}


if(alloc_failed)
{

/* Libere tout ce qui a ete alloue pour cette tentative (sauf block_list[0], qui appartenait deja a l'inode avant cet appel). */

for(u32 i=1; i<blocks_needed; i++)
{

if(block_list[i] != 0) { block_free(block_list[i]); }

}

return 0;

}


/* Ecrit chaque morceau de donnees dans son bloc. */

for(u32 i=0; i<blocks_needed; i++)
{

u8 chunk[BLOCK_SIZE];

for(u32 j=0; j<BLOCK_SIZE; j++) { chunk[j] = 0; }


u32 offset = i * BLOCK_SIZE;

u32 remaining = length - offset;

u32 chunk_len = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;


mk_memcpy(chunk, data + offset, chunk_len);


block_write(block_list[i], chunk);

}


/* Ecrit le bloc indirect (la liste des numeros de blocs). */

block_write(new_indirect, (u8*)block_list);


node->indirect_block = new_indirect;

node->size = length;


/* Correctif (persistance disque) : voir le commentaire de file_write() plus haut -- meme raisonnement exactement. */

inode_table_save();


return 1;


}



u32 file_read_bin(
char* name,
u8* out_buffer,
u32 buffer_capacity
)
{


inode* node;

node = inode_find(name);


if(node==0 || node->deleted)
{

return 0;

}


if(node->size > buffer_capacity)
{

return 0;

}


/* Chemin COURT : un seul bloc, comme file_read(). */

if(node->indirect_block == 0)
{

u8 buffer[BLOCK_SIZE];

block_read(node->block, buffer);

mk_memcpy(out_buffer, buffer, node->size);

return node->size;

}


/* Chemin LONG : relit le bloc indirect, puis chaque bloc de donnees qu'il reference, dans l'ordre. */

u8 indirect_buffer[BLOCK_SIZE];

block_read(node->indirect_block, indirect_buffer);

u32* block_list = (u32*)indirect_buffer;

u32 max_entries = BLOCK_SIZE / sizeof(u32);


u32 remaining = node->size;

u32 out_offset = 0;


for(u32 i=0; i<max_entries && remaining > 0; i++)
{

if(block_list[i] == 0) { break; }


u8 chunk[BLOCK_SIZE];

block_read(block_list[i], chunk);


u32 chunk_len = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;

mk_memcpy(out_buffer + out_offset, chunk, chunk_len);


out_offset += chunk_len;

remaining -= chunk_len;

}


return node->size;


}
