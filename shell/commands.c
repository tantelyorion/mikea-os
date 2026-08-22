#include "commands.h"

#include "../kernel/drivers/power/power.h"

#include "msh.h"

#include "../libc/string.h"

#include "../security/user.h"

#include "../security/permission.h"

#include "../packages/mpm.h"

#include "../filesystem/file.h"

#include "../filesystem/directory.h"

#include "../filesystem/inode.h"

#include "../gui/gui.h"

#include "../kernel/drivers/graphics/graphics.h"

#include "../kernel/drivers/mouse/mouse.h"

#include "../boot/installer/installer.h"

#include "../apps/calculator/calculator.h"

#include "../apps/file_manager/file_manager.h"

#include "../apps/session/session.h"

#include "../apps/settings/settings.h"

#include "../kernel/process/process.h"

#include "../mkx/mkx.h"



void console_write(
    const char* text
);


void console_clear();


void keyboard_flush();


int keyboard_available();

char keyboard_getchar();


unsigned long timer_ticks();


void input_readline(
char* buffer,
unsigned int max_len
);


void input_readline_secure(
char* buffer,
unsigned int max_len
);



/*
    Correctif majeur (historique) : execute_command() ne
    comparait que le PREMIER caractere de la commande saisie
    (command[0]) -- "clear" et "cpu" partageaient le meme
    branchement, et n'importe quel mot commencant par une
    lettre connue etait accepte comme commande valide. On
    compare desormais la chaine complete avec mk_strcmp().

    Correctif complementaire (celui-ci) : plusieurs fonctions
    C completes et fonctionnelles existaient sans jamais etre
    reliees a une commande shell -- gestion des paquets
    (packages/mpm.c), fichiers et repertoires
    (filesystem/file.c, directory.c), creation d'utilisateurs
    (security/user.c). On ajoute ici les commandes
    correspondantes, avec une analyse d'arguments minimale
    (decoupage par espaces) qui n'existait pas non plus :
    l'ancienne version ne comparait que des commandes sans
    aucun argument.
*/


/*
    Copie le prochain "mot" (separe par des espaces) de
    *cursor vers dest (borne a dest_size-1 caracteres), avance
    *cursor apres ce mot, et renvoie la longueur copiee (0 si
    aucun mot n'a ete trouve avant la fin de la chaine).
*/

static u32 next_token(
char* dest,
u32 dest_size,
char** cursor
)
{

char* c = *cursor;


while(*c == ' ')
{

c++;

}


u32 i = 0;


while(c[i] != 0 && c[i] != ' ' && i < dest_size - 1)
{

dest[i] = c[i];

i++;

}


dest[i] = 0;


*cursor = c + i;


return i;

}



static void cmd_useradd(
char* args
)
{


/*
    Seul root (id 1, voir security/user.c :
    user_system_init()) peut creer un compte. Il n'existe
    pour l'instant aucun autre niveau d'habilitation "gestion
    des utilisateurs" dans security/permission.h -- une piste
    d'amelioration future si des roles plus fins sont
    necessaires.
*/

user* current = user_get_current();


if(current == 0 || current->id != 1)
{

console_write("Permission denied: only root can create users\n");

return;

}


char username[32];


if(next_token(username, sizeof(username), &args) == 0)
{

console_write("Usage: useradd <username>\n");

return;

}


/*
    Correctif (compte fantome) : user_create() ne verifiait
    jamais si le nom demande existait deja -- contrairement a
    inode_create()/directory_create() (filesystem/), qui
    refusent tous les deux un nom deja pris. Retaper deux fois
    "useradd bob" par erreur creait donc silencieusement DEUX
    comptes distincts portant le meme nom : user_login()/
    user_find() ne trouvant jamais que le premier (ils
    s'arretent au premier resultat), le second devenait un
    compte fantome, invisible et impossible a administrer
    (passwd/userdel le manqueraient aussi), qui occupe
    definitivement une place dans la table (voir
    security/user.h, user_slot_count()).
*/

if(user_find(username) != 0)
{

console_write("User already exists\n");

return;

}


/*
    Correctif : le mot de passe etait auparavant lu comme un
    deuxieme mot sur la meme ligne de commande ("useradd bob
    secret"), donc affiche en clair a l'ecran (et potentiel-
    lement visible dans un historique de commandes si l'on en
    ajoute un un jour). On le demande desormais separement,
    avec le meme masquage par etoiles que l'ecran de connexion
    (security/login.c, input_readline_secure()).
*/

char password[32];


console_write("New password: ");

input_readline_secure(password, sizeof(password));


user* created = user_create(username, password);

if(created != 0)
{

/*
    Correctif : security/permission.c refuse tout par
    defaut a un utilisateur qui n'a recu aucune permission
    explicite (permission_table[id] = 0, voir
    permission_init()). user_create() ne touchait jamais
    cette table : un compte fraichement cree pouvait donc
    se connecter (whoami fonctionnait) mais aucune commande
    de fichiers (mkdir/touch/write/rm) ne fonctionnait,
    filesystem/file.c les refusant systematiquement. On
    accorde ici les droits d'un utilisateur normal (lecture
    + ecriture, mais pas execution -- reservee a root pour
    l'instant, voir permission_init()).
*/

permission_grant(created->id, PERMISSION_READ | PERMISSION_WRITE);

console_write("User created\n");

}
else
{

console_write("Failed to create user (table full?)\n");

}


}



static void cmd_passwd(
char* args
)
{


/*
    Sans argument : l'utilisateur change son propre mot de
    passe. Avec un nom d'utilisateur en argument : seul root
    peut changer le mot de passe d'un AUTRE compte (un
    utilisateur normal ne peut donc reinitialiser que le
    sien).
*/

user* current = user_get_current();

if(current == 0)
{

console_write("No user logged in\n");

return;

}


char target_name[32];

user* target;

if(next_token(target_name, sizeof(target_name), &args) == 0)
{

target = current;

}
else
{

if(current->id != 1 && mk_strcmp(target_name, current->username) != 0)
{

console_write("Permission denied: only root can change another user's password\n");

return;

}

target = user_find(target_name);

if(target == 0)
{

console_write("No such user\n");

return;

}

}


char new_password[32];

console_write("New password: ");

input_readline_secure(new_password, sizeof(new_password));

user_set_password(target, new_password);

console_write("Password updated\n");


}



static void cmd_userdel(
char* args
)
{


/*
    Meme regle que useradd : seul root gere les comptes. Le
    compte root lui-meme est protege par user_delete()
    (security/user.c), mais on le refuse deja ici pour donner
    un message d'erreur explicite plutot qu'un "No such user"
    trompeur.
*/

user* current = user_get_current();

if(current == 0 || current->id != 1)
{

console_write("Permission denied: only root can delete users\n");

return;

}


char username_to_delete[32];

if(next_token(username_to_delete, sizeof(username_to_delete), &args) == 0)
{

console_write("Usage: userdel <username>\n");

return;

}

if(mk_strcmp(username_to_delete, "root") == 0)
{

console_write("Cannot delete root\n");

return;

}

if(user_delete(username_to_delete))
{

console_write("User deleted\n");

}
else
{

console_write("No such user\n");

}


}



static void cmd_users()
{


/*
    Liste tous les comptes actifs (le mot de passe reste
    hache, donc rien de sensible n'est expose ici), avec un
    "*" devant le compte actuellement connecte -- utile des
    que plusieurs comptes existent, ce qu'aucune commande ne
    permettait de verifier jusqu'ici.
*/

user* current = user_get_current();

u32 count = user_slot_count();

for(u32 i = 0; i < count; i++)
{

user* u = user_get_slot(i);

if(u == 0 || !u->active)
{

continue;

}

if(current != 0 && u->id == current->id)
{

console_write("* ");

}
else
{

console_write("  ");

}

console_write(u->username);

console_write("\n");

}


}



static const char* process_state_name(process_state s)
{

if(s == PROCESS_READY)
{

return "ready";

}

if(s == PROCESS_RUNNING)
{

return "running";

}

return "stopped";

}



static void cmd_ps()
{


/*
    Correctif : kernel/process/process.c (table des
    processus) etait initialise au demarrage
    (process_init()) mais process_create()/process_get()
    n'etaient appeles nulle part -- une table toujours vide,
    invisible et inutile. kernel.c enregistre desormais le
    shell comme processus "msh" avant de le lancer comme
    thread ; cette commande rend cette table enfin visible.
*/

u32 count = process_get_count();

if(count == 0)
{

console_write("  (aucun processus enregistre)\n");

return;

}

for(u32 pid = 1; pid <= count; pid++)
{

process* p = process_get(pid);

if(p == 0)
{

continue;

}

console_write("  ");

console_write(p->name);

console_write(" (");

console_write(process_state_name(p->state));

console_write(")\n");

}


}



static void cmd_mpm(
char* args
)
{


char subcommand[16];


if(next_token(subcommand, sizeof(subcommand), &args) == 0)
{

console_write("Usage: mpm <install|remove|list> [package]\n");

return;

}


if(mk_strcmp(subcommand, "list") == 0)
{

mpm_list();

return;

}


char package[32];


if(next_token(package, sizeof(package), &args) == 0)
{

console_write("Usage: mpm <install|remove> <package>\n");

return;

}


if(mk_strcmp(subcommand, "install") == 0)
{

mpm_install(package);

}
else if(mk_strcmp(subcommand, "remove") == 0)
{

mpm_remove(package);

}
else
{

console_write("Usage: mpm <install|remove|list> [package]\n");

}


}



static void cmd_mkdir(
char* args
)
{


char name[32];


if(next_token(name, sizeof(name), &args) == 0)
{

console_write("Usage: mkdir <name>\n");

return;

}


if(directory_create(name, ""))
{

console_write("Directory created\n");

}
else
{

console_write("Could not create directory (name empty or already used)\n");

}


}



static void cmd_touch(
char* args
)
{


char name[32];


if(next_token(name, sizeof(name), &args) == 0)
{

console_write("Usage: touch <name>\n");

return;

}


if(file_create(name))
{

console_write("File created\n");

}
else
{

console_write("Could not create file (permission or name already used)\n");

}


}



static void cmd_write(
char* args
)
{


char name[32];


if(next_token(name, sizeof(name), &args) == 0)
{

console_write("Usage: write <name> <text>\n");

return;

}


/*
    Contrairement aux autres commandes, le contenu du
    fichier peut lui-meme contenir des espaces : on ne
    tokenize pas le reste de la ligne, on saute juste les
    espaces qui separent le nom du contenu.
*/

while(*args == ' ')
{

args++;

}


if(*args == 0)
{

console_write("Usage: write <name> <text>\n");

return;

}


if(file_write(name, args))
{

console_write("File written\n");

}
else
{

console_write("Could not write file (permission denied)\n");

}


}



static void cmd_cat(
char* args
)
{


char name[32];


if(next_token(name, sizeof(name), &args) == 0)
{

console_write("Usage: cat <name>\n");

return;

}


char* content = file_read(name);


if(content == 0)
{

console_write("File not found\n");

return;

}


console_write(content);

console_write("\n");


}



static void cmd_ls()
{


/*
    Correctif : aucune commande ne permettait de voir les
    fichiers deja crees (touch/write/mkdir n'avaient aucun
    moyen de verifier leur propre resultat). inode_get() et
    MAX_INODES existaient deja (filesystem/inode.c) mais
    n'etaient utilises par aucune commande shell.
*/

u32 count = 0;


for(u32 id = 1; id <= MAX_INODES; id++)
{

inode* node = inode_get(id);

/*
    Correctif (fichiers "supprimes" encore listes) : sans ce
    filtre, un fichier deplace vers la corbeille (voir
    filesystem/file.c, file_trash()) continuait d'apparaitre
    ici normalement -- inode_get() ne filtre que sur "used",
    pas sur "deleted". Utilisez "trashls" pour voir le
    contenu de la corbeille.
*/

if(node != 0 && !node->deleted)
{

console_write("  ");

console_write(node->name);

console_write("\n");

count++;

}

}


if(count == 0)
{

console_write("  (empty)\n");

}


}



static void cmd_run(
char* args
)
{


/*
    Correctif : mkx/loader.c fournit mkx_execute(), qui
    execute reellement le point d'entree d'un programme MKX
    (voir la correctif documente dans ce fichier), mais rien
    dans le noyau ne l'appelait jamais -- tout le format MKX
    etait mort code, inaccessible depuis le shell.

    Limite connue, deja documentee dans mkx/loader.c et
    sdk/mikea_sdk.h : il n'existe pas de separation
    memoire/privilege (pas de Ring 3) ni d'interface d'appel
    systeme reelle. Un programme MKX charge ici s'execute au
    meme niveau de privilege que le noyau, et ne peut appeler
    des fonctions du noyau (console_write, mk_malloc...) que
    s'il a ete compile et lie en connaissant leurs adresses
    reelles dans cette image du noyau -- il n'existe pas
    encore d'outil pour produire un tel fichier .mkx
    automatiquement (a la difference de apps/hello/hello.c,
    qui montre le SDK cote source mais n'est pas encore
    compile/empaquete par le Makefile). "run" complete donc le
    chemin noyau -> chargeur, la production reelle de
    fichiers .mkx reste un travail a part.
*/

char name[32];

if(next_token(name, sizeof(name), &args) == 0)
{

console_write("Usage: run <name>\n");

return;

}

char* content = file_read(name);

if(content == 0)
{

console_write("File not found\n");

return;

}

mkx_header* program = (mkx_header*)content;

/*
    Correctif stabilite : file_read() renvoie un pointeur
    vers un tampon statique de MAX_FILE_SIZE octets
    (filesystem/file.c, read_buffer) -- c'est cette taille
    reelle qu'il faut transmettre a mkx_execute() pour qu'il
    puisse rejeter un en-tete MKX qui declarerait un
    programme plus grand que ce tampon (voir mkx/mkx.h).
*/

mkx_execute(program, MAX_FILE_SIZE);


}



/*
    Complement (message de demarrage introuvable) : le statut
    VBE/graphique n'etait visible qu'une seule fois, tres tot au
    demarrage (graphics_init(), voir kernel/kernel.c) -- avec le
    nombre de messages de diagnostic desormais affiches avant
    l'ecran de connexion, ce message defile hors de l'ecran
    (VGA texte, sans historique de defilement) avant meme
    d'etre lisible. Cette commande rend ce statut consultable a
    tout moment, une fois connecte.
*/

static void cmd_gfxstatus()
{


if(gfx_available())
{

console_write("Graphique (VBE) : actif\n");

}
else
{

console_write("Graphique (VBE) : indisponible -- mode texte uniquement\n");

}


}



/*
    Petit helper de conversion entier signe -> texte, absent du
    reste du projet (libc/string.c ne fournit que des fonctions
    de chaines, pas de conversion numerique) -- necessaire pour
    afficher la position de la souris.
*/

static void write_int(s32 value)
{

char buffer[12];

int i = 0;

int negative = 0;


if (value < 0)
{

negative = 1;

value = -value;

}


if (value == 0)
{

buffer[i++] = '0';

}


while (value > 0)
{

buffer[i++] = (char)('0' + (value % 10));

value /= 10;

}


if (negative)
{

console_write("-");

}


while (i > 0)
{

i--;

char c[2];

c[0] = buffer[i];

c[1] = 0;

console_write(c);

}

}


/*
    Test du pilote souris PS/2 (kernel/drivers/mouse) : affiche
    des lectures successives de la position et des boutons,
    espacees par timer_ticks(), pour verifier que le
    deplacement de la souris est bien capte -- deplacez la
    souris pendant que cette commande tourne.
*/

static void cmd_mouse()
{


console_write("Statut initialisation : ");

console_write(mouse_get_init_status());

console_write("\n");


if (!gfx_available())
{

console_write("Souris : le pilote fonctionne independamment du mode graphique,\n");

console_write("mais la position n'est utile qu'en mode graphique (voir gfxstatus).\n");

}


console_write("Deplacez la souris... (10 lectures)\n");


for (int sample = 0; sample < 10; sample++)
{


unsigned long start = timer_ticks();

while (timer_ticks() - start < 20)
{

/* Attente active bornee (~200ms a 100Hz) entre deux lectures. */

}


console_write("x=");

write_int(mouse_get_x());

console_write(" y=");

write_int(mouse_get_y());

console_write(" boutons: ");

console_write(mouse_left_pressed() ? "G " : ". ");

console_write(mouse_right_pressed() ? "D " : ". ");

console_write(mouse_middle_pressed() ? "M" : ".");

console_write("\n");


}


}



/*
    Installe MikeaOS sur le disque de donnees (voir
    boot/installer/installer.c) : copie l'image de demarrage
    du disque maitre (celui sur lequel le systeme tourne
    actuellement) vers le disque esclave, avec verification
    immediate de chaque secteur ecrit.

    Operation destructive et reservee a root (comme
    "useradd"/"userdel") : exige une confirmation explicite
    ("OUI" en toutes lettres, pas juste Entree) avant de
    commencer.
*/

static void cmd_install()
{


user* current = user_get_current();

if (current == 0 || current->id != 1)
{

console_write("Permission denied: reserve a root\n");

return;

}


console_write("ATTENTION : ceci va EFFACER le disque de donnees actuel\n");

console_write("et le remplacer par une copie demarrable de MikeaOS.\n");

console_write("Tapez OUI (en majuscules) pour confirmer, autre chose pour annuler :\n");


char confirm[8];

keyboard_flush();

input_readline(confirm, sizeof(confirm));


if (mk_strcmp(confirm, "OUI") != 0)
{

console_write("Installation annulee.\n");

return;

}


console_write("Installation en cours (2048 secteurs, 1 Mo)...\n");


install_result result = installer_run((void*)0);


if (result == INSTALL_OK)
{

console_write("Installation reussie et verifiee.\n");

console_write("Pour demarrer dessus : dans QEMU, inversez l'ordre des deux\n");

console_write("'-drive' (le disque installe doit passer en premier).\n");

}
else
{

console_write("Installation echouee -- voir le message d'erreur ci-dessus.\n");

}


}


/*
    Les applications systeme par defaut (calculatrice,
    explorateur de fichiers, session/"gui", parametres)
    vivent desormais dans leurs propres fichiers sous apps/,
    pour une meilleure organisation -- toujours compilees
    directement dans le noyau (voir apps/calculator/calculator.c,
    apps/file_manager/file_manager.c, apps/session/session.c,
    apps/settings/settings.c), appelees ici comme n'importe
    quelle autre commande shell.
*/



static void cmd_mv(
char* args
)
{


char old_name[32];

char new_name[32];


if(next_token(old_name, sizeof(old_name), &args) == 0
|| next_token(new_name, sizeof(new_name), &args) == 0)
{

console_write("Usage: mv <old_name> <new_name>\n");

return;

}


if(file_rename(old_name, new_name))
{

console_write("File renamed\n");

}
else
{

console_write("File not found, name already used, or permission denied\n");

}


}



static void cmd_rm(
char* args
)
{


char name[32];


if(next_token(name, sizeof(name), &args) == 0)
{

console_write("Usage: rm <name>\n");

return;

}


/*
    "rm" reste une suppression IMMEDIATE et definitive (meme
    semantique que le "rm" Unix, pour ne pas changer par
    surprise le comportement d'une commande deja existante) --
    voir "trash" ci-dessous pour une suppression recuperable.
*/

if(file_delete(name))
{

console_write("File removed\n");

}
else
{

console_write("File not found or permission denied\n");

}


}



/*
    ====================================================
    Corbeille (voir filesystem/file.c)
    ====================================================

    Contrairement a "rm" ci-dessus (suppression immediate et
    definitive), "trash" deplace un fichier vers la corbeille :
    recuperable par "restore" tant que "emptytrash" n'a pas ete
    lancee. Meme fonctionnalite que le bouton "Supprimer" de
    l'explorateur graphique (apps/file_manager/file_manager.c),
    exposee ici en ligne de commande pour rester coherente avec
    le reste du shell (chaque action de fichier a deja son
    equivalent "rm"/"cat"/"touch").
*/


static void cmd_trash(
char* args
)
{


char name[32];


if(next_token(name, sizeof(name), &args) == 0)
{

console_write("Usage: trash <name>\n");

return;

}


if(file_trash(name))
{

console_write("File moved to trash\n");

}
else
{

console_write("File not found, already in trash, or permission denied\n");

}


}



static void cmd_restore(
char* args
)
{


char name[32];


if(next_token(name, sizeof(name), &args) == 0)
{

console_write("Usage: restore <name>\n");

return;

}


if(file_restore(name))
{

console_write("File restored\n");

}
else
{

console_write("File not found, not in trash, or permission denied\n");

}


}



static void cmd_trashls()
{


u32 count = 0;


for(u32 id = 1; id <= MAX_INODES; id++)
{

inode* node = inode_get(id);

if(node != 0 && node->deleted)
{

console_write("  ");

console_write(node->name);

console_write("\n");

count++;

}

}


if(count == 0)
{

console_write("Trash is empty\n");

}


}



static void cmd_emptytrash()
{


u32 removed = file_empty_trash();


char count_str[12];

int i = 0;

if(removed == 0)
{

count_str[i++] = '0';

}
else
{

u32 v = removed;

char tmp[12];

int t = 0;

while(v > 0)
{

tmp[t++] = (char)('0' + (v % 10));

v /= 10;

}

while(t > 0)
{

t--;

count_str[i++] = tmp[t];

}

}

count_str[i] = 0;


console_write("Trash emptied (");

console_write(count_str);

console_write(" file(s) permanently deleted)\n");


}



void execute_command(
    char* command
)
{


if(mk_strcmp(command, "help") == 0)
{

console_write("Available commands:\n");
console_write("help\n");
console_write("about\n");
console_write("version\n");
console_write("clear\n");
console_write("cpu\n");
console_write("mem\n");
console_write("whoami\n");
console_write("useradd <user>\n");
console_write("userdel <user>\n");
console_write("passwd [user]\n");
console_write("users\n");
console_write("logout\n");
console_write("ps\n");
console_write("mpm <install|remove|list> [package]\n");
console_write("mkdir <name>\n");
console_write("touch <name>\n");
console_write("write <name> <text>\n");
console_write("cat <name>\n");
console_write("rm <name>\n");
console_write("mv <old_name> <new_name>\n");
console_write("trash <name>\n");
console_write("restore <name>\n");
console_write("trashls\n");
console_write("emptytrash\n");
console_write("ls\n");
console_write("gui\n");
console_write("gfxstatus\n");
console_write("mouse\n");
console_write("install\n");
console_write("calc\n");
console_write("files\n");
console_write("settings\n");
console_write("run <name>\n");
console_write("reboot\n");
console_write("shutdown\n");


}


else if(mk_strcmp(command, "about") == 0)
{

console_write("Mikea OS\n");
console_write("Open Source Operating System\n");
console_write("Developer : Tantely Orion\n");

}


else if(mk_strcmp(command, "version") == 0)
{

console_write("Mikea OS version 0.1.5\n");

}


else if(mk_strcmp(command, "clear") == 0)
{

/*
    Correctif : "clear" se contentait d'afficher le texte
    "Screen cleared" sans jamais effacer l'ecran, puis (une
    fois fb_clear() branchee) sans jamais remettre le
    curseur en haut a gauche. On appelle desormais
    console_clear() (kernel/console/console.c), qui fait les
    deux.
*/

console_clear();

console_write("Screen cleared\n");

}


else if(mk_strcmp(command, "cpu") == 0)
{

console_write("CPU Core             : READY\n");

}


else if(mk_strcmp(command, "mem") == 0)
{

console_write("Memory Manager OK\n");

}


else if(mk_strcmp(command, "whoami") == 0)
{

/*
    Correctif : aucune commande ne permettait de savoir
    quel utilisateur etait connecte, alors meme que le
    systeme de connexion (security/login.c) est desormais
    obligatoire au demarrage.
*/

user* current = user_get_current();

if(current == 0)
{

console_write("(no user)\n");

}
else
{

console_write(current->username);

console_write("\n");

}

}


else if(mk_strncmp(command, "useradd ", 8) == 0)
{

cmd_useradd(command + 8);

}


else if(mk_strncmp(command, "userdel ", 8) == 0)
{

cmd_userdel(command + 8);

}


else if(mk_strcmp(command, "passwd") == 0)
{

cmd_passwd(command + 6);

}


else if(mk_strncmp(command, "passwd ", 7) == 0)
{

cmd_passwd(command + 7);

}


else if(mk_strcmp(command, "users") == 0)
{

cmd_users();

}


else if(mk_strcmp(command, "ps") == 0)
{

cmd_ps();

}


else if(mk_strcmp(command, "logout") == 0)
{

/*
    Correctif : rien ne permettait de quitter une session
    ouverte sans redemarrer le noyau. shell_request_logout()
    (voir shell/msh.h) pose un drapeau lu par la boucle
    principale de msh_start(), qui revient alors a
    login_prompt().
*/

console_write("Logging out...\n");

shell_request_logout();

}


else if(mk_strncmp(command, "mpm ", 4) == 0)
{

cmd_mpm(command + 4);

}


else if(mk_strncmp(command, "mkdir ", 6) == 0)
{

cmd_mkdir(command + 6);

}


else if(mk_strncmp(command, "touch ", 6) == 0)
{

cmd_touch(command + 6);

}


else if(mk_strncmp(command, "write ", 6) == 0)
{

cmd_write(command + 6);

}


else if(mk_strncmp(command, "cat ", 4) == 0)
{

cmd_cat(command + 4);

}


else if(mk_strcmp(command, "ls") == 0)
{

cmd_ls();

}


else if(mk_strcmp(command, "gui") == 0)
{

cmd_gui();

}


else if(mk_strcmp(command, "gfxstatus") == 0)
{

cmd_gfxstatus();

}


else if(mk_strcmp(command, "mouse") == 0)
{

cmd_mouse();

}


else if(mk_strcmp(command, "install") == 0)
{

cmd_install();

}


else if(mk_strcmp(command, "calc") == 0)
{

cmd_calc();

}


else if(mk_strcmp(command, "files") == 0)
{

cmd_files();

}


else if(mk_strcmp(command, "settings") == 0)
{

cmd_settings();

}


else if(mk_strncmp(command, "run ", 4) == 0)
{

cmd_run(command + 4);

}


else if(mk_strncmp(command, "rm ", 3) == 0)
{

cmd_rm(command + 3);

}


else if(mk_strncmp(command, "mv ", 3) == 0)
{

cmd_mv(command + 3);

}


else if(mk_strncmp(command, "trash ", 6) == 0)
{

cmd_trash(command + 6);

}


else if(mk_strncmp(command, "restore ", 8) == 0)
{

cmd_restore(command + 8);

}


else if(mk_strcmp(command, "trashls") == 0)
{

cmd_trashls();

}


else if(mk_strcmp(command, "emptytrash") == 0)
{

cmd_emptytrash();

}


else if(mk_strcmp(command, "reboot") == 0)
{

power_reboot();

}


else if(mk_strcmp(command, "shutdown") == 0)
{

power_shutdown();

}


else if(command[0] == 0)
{

/* Ligne vide : ne rien afficher, pas d'erreur. */

}


else
{

console_write("Command not found\n");

}


}
