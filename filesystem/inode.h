/*
====================================================

        Mikea OS Filesystem

        Inode Manager

        MKFS v2


        Developer:
        Tantely Orion


====================================================
*/


#ifndef MIKEA_INODE_H
#define MIKEA_INODE_H


#include "../include/types.h"



/*
====================================================
CONFIGURATION
====================================================
*/


#define MAX_INODES 128



#define FILE_NAME_SIZE 32




/*
====================================================
INODE STRUCTURE
====================================================
*/


typedef struct
{


/*
Unique identifier

*/

u32 id;



/*
File name

*/

char name[FILE_NAME_SIZE];



/*
File size

*/

u32 size;



/*
First data block

*/

u32 block;


/*
    Correctif (fichiers multi-blocs, voir fs_layout.h et
    file.c, file_write_bin()/file_read_bin()) : "block"
    ci-dessus reste utilise seul pour les petits fichiers (au
    plus 512 octets, un seul bloc -- chemin inchange, voir
    file_write()/file_read()). Au-dela, ce champ pointe vers un
    "bloc indirect" : un bloc de donnees ordinaire qui, au lieu
    de contenir les donnees du fichier, contient une simple
    liste de u32 -- les numeros des blocs qui, mis bout a bout,
    contiennent les donnees reelles. 512 octets / 4 octets par
    entree = 128 blocs de donnees adressables, soit 65536
    octets (64 Ko) de taille de fichier maximale -- au-dela,
    file_write_bin() echoue proprement (voir son commentaire).
    0 = fichier a un seul bloc (comme avant ce correctif).
*/

u32 indirect_block;



/*
File status

*/

int used;



/*
    Champ reserve pour un futur systeme de permissions PAR
    FICHIER (actuellement fixe a 7 = rwx a la creation, voir
    inode_create() dans inode.c, et jamais relu ailleurs).

    Le controle d'acces reellement applique aujourd'hui est
    global PAR UTILISATEUR (security/permission.c,
    filesystem/file.c::current_user_can() /
    filesystem/directory.c::current_user_can()) : un
    utilisateur avec PERMISSION_WRITE peut modifier n'importe
    quel fichier -- il n'existe pas encore de notion de
    "proprietaire" par inode. Ne pas considerer ce champ comme
    applique tant qu'un tel modele (avec un champ owner_id)
    n'aura pas ete ajoute et branche dans file.c/directory.c.

Permission

*/

u32 permission;



/*
    Corbeille (voir filesystem/file.c : file_trash(),
    file_restore(), file_empty_trash()) : 1 si le fichier a
    ete "supprime" via la corbeille (contenu et bloc de
    donnees encore intacts, juste masque des listages
    normaux), 0 sinon. N'a aucun rapport avec "used"
    ci-dessus : un inode "deleted" reste "used" (l'espace
    disque n'est libere qu'a la suppression definitive, via
    inode_delete()). Remis a 0 systematiquement a la
    creation/reutilisation d'un inode (voir inode_create()
    dans inode.c).

Deleted (corbeille)

*/

int deleted;



/*
    Dossiers (voir filesystem/directory.c) : 1 si cet inode
    represente un dossier, 0 si c'est un fichier normal.
*/

int is_directory;


/*
    Nom du dossier parent ("" = racine). Sert uniquement a la
    NAVIGATION (voir directory_list_children(), et
    apps/file_manager/file_manager.c) -- les noms restent
    globalement uniques sur tout le systeme de fichiers, meme
    dans des dossiers differents : inode_find() ne recherche
    encore que par nom seul, sans tenir compte du dossier
    (limite assumee de ce modele "table plate + parent" plutot
    qu'une vraie resolution de chemin "/a/b/c", qui exigerait
    de faire evoluer inode_find() et tous ses appelants
    existants pour raisonner par (parent, nom) plutot que par
    nom seul).
*/

char parent[FILE_NAME_SIZE];



} inode;





/*
====================================================
API
====================================================
*/


void inode_init();



inode* inode_create(
char* name
);


/*
    Variante etendue de inode_create() : permet de preciser un
    dossier parent et de marquer l'inode comme un dossier.
    inode_create(name) ci-dessus reste un simple raccourci vers
    inode_create_ex(name, "", 0) (creation a la racine, fichier
    normal) -- tous les appels existants continuent de se
    comporter exactement comme avant.
*/

inode* inode_create_ex(
char* name,
char* parent,
int is_directory
);



inode* inode_find(
char* name
);



void inode_delete(
char* name
);



inode* inode_get(
u32 id
);



u32 inode_count();



/*
    Sauvegarde/recharge la table des inodes (inode_table, privee
    a ce fichier) sur/depuis le vrai disque -- voir fs_layout.h
    (FS_INODE_TABLE_START_BLOCK) et mkfs.c pour l'orchestration
    complete. Correctif persistance disque (voir le commentaire
    de fs_layout.h) : c'est cette table qui manquait pour que
    les fichiers survivent reellement a un redemarrage -- leur
    CONTENU (filesystem/block.c) etait deja ecrit sur le vrai
    disque, mais devenait orphelin sans elle.
*/

void inode_table_save();

void inode_table_load();



#endif