#include "directory.h"

#include "inode.h"

#include "../security/user.h"

#include "../security/permission.h"



void directory_init()
{


}



/*
    Meme regle que filesystem/file.c (current_user_can()) :
    hors contexte utilisateur (aucune connexion active, ex.
    pendant l'initialisation du noyau), on autorise ; sinon on
    applique la table de permissions reelle.

    Correctif : directory_create() n'appliquait jusqu'ici
    AUCUN controle d'acces, alors que file_create()/file_write()/
    file_delete() (filesystem/file.c) exigent tous les trois
    PERMISSION_WRITE. Un utilisateur sans droit d'ecriture
    pouvait donc quand meme creer des "repertoires" a volonte
    -- une incoherence entre deux operations d'ecriture sur le
    meme systeme de fichiers.
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
    Correctif : directory_create() etait un stub vide qui
    ignorait totalement son parametre "name" -- creer un
    "repertoire" n'avait donc aucun effet observable.

    Limite connue (documentee ici volontairement) : le
    systeme de fichiers ne modelise pour l'instant qu'une
    liste plate d'inodes (voir filesystem/inode.c), sans
    hierarchie de chemins ("/a/b/c") ni bit "est un
    repertoire" dans la structure inode. Une vraie
    arborescence (inode dedie aux repertoires, entrees
    parent/enfants, resolution de chemin) reste un travail
    futur plus consequent. En attendant, on reserve au moins
    un inode reel pour le nom du repertoire, au lieu de ne
    rien faire du tout : cela permet deja de detecter les
    doublons de nom (inode_find()) entre fichiers et
    "repertoires".
*/

int directory_create(
char* name,
char* parent
)
{


if(!current_user_can(PERMISSION_WRITE))
{

return 0;

}


if(name == 0 || name[0] == 0)
{

return 0;

}


if(inode_find(name) != 0)
{

/* Existe deja (fichier ou repertoire). */

return 0;

}


inode* node = inode_create_ex(name, parent, 1);


if(node == 0)
{

return 0;

}


return 1;


}



u32 directory_list_children(
char* parent,
char out_names[][FILE_NAME_SIZE],
int* out_is_dir,
u32 max_count
)
{


u32 count = 0;


for(u32 id = 1; id <= MAX_INODES && count < max_count; id++)
{


inode* node = inode_get(id);


if(node == 0 || node->deleted)
{

continue;

}


/*
    Comparaison manuelle (pas de mk_strcmp ici : ce fichier
    n'inclut pas libc/string.h, voir la meme approche
    caractere-par-caractere dans shell/msh.c) entre le
    parent demande et celui de l'inode.
*/

int same_parent = 1;

u32 p = 0;

while(parent[p] || node->parent[p])
{

if(parent[p] != node->parent[p])
{

same_parent = 0;

break;

}

p++;

}


if(!same_parent)
{

continue;

}


u32 j = 0;

while(node->name[j] && j < FILE_NAME_SIZE - 1)
{

out_names[count][j] = node->name[j];

j++;

}

out_names[count][j] = 0;


out_is_dir[count] = node->is_directory;


count++;


}


return count;


}