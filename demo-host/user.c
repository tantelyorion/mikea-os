#include "user.h"

#include "password.h"

#include "timer.h"



static user users[MAX_USERS];


static u32 user_count = 0;


static user* current_user = 0;



/*
    Compare deux chaines terminees par 0 et renvoie 1
    si elles sont strictement identiques (meme longueur
    incluse), 0 sinon.

    Correctif securite : l'ancienne version de user_login()
    ne bouclait que sur la longueur de la chaine saisie par
    l'utilisateur, sans jamais verifier que le nom stocke ne
    contenait pas des caracteres supplementaires. Un simple
    prefixe (ex. "r" au lieu de "root") etait alors accepte
    comme identifiant valide.
*/

static int strings_equal(const char* a, const char* b)
{

u32 i = 0;


while (a[i] != 0 && b[i] != 0)
{

if (a[i] != b[i])
{
return 0;
}

i++;

}


/* Les deux doivent se terminer au meme endroit. */

return a[i] == b[i];

}



void user_system_init()
{


for(int i=0;i<MAX_USERS;i++)
{


users[i].active=0;


}


user_count=0;



/*
Default user

root

*/


user_create(
"root",
"mikea"
);


}





user* user_create(
char* username,
char* password
)
{


if(user_count>=MAX_USERS)
{

return 0;

}



user* u;


u=&users[user_count];



u->id=
user_count+1;



u->active=1;



int i=0;


while(username[i] && i<31)
{


u->username[i]=username[i];


i++;

}


u->username[i]=0;



/*
    Correctif securite : le mot de passe n'est plus
    copie en clair. On genere un sel (a partir du
    compteur de tick systeme et de l'id utilisateur,
    faute d'un generateur aleatoire materiel dans le
    noyau pour l'instant) puis on stocke uniquement
    le hash sale.
*/

u32 salt = (u32)timer_ticks() ^ (u->id * 2654435761u);

u->password_salt = salt;

u->password_hash = password_hash(password, salt);


user_count++;



return u;


}





user* user_login(
char* username,
char* password
)
{


for(int i=0;i<MAX_USERS;i++)
{


if(users[i].active)
{


if(strings_equal(users[i].username, username))
{


u64 hash = password_hash(password, users[i].password_salt);


if(hash == users[i].password_hash)
{

current_user = &users[i];

return &users[i];

}


}


}


}



return 0;


}



user* user_get_current()
{

return current_user;

}



void user_logout()
{

current_user = 0;

}



user* user_find(
char* username
)
{

for(int i=0;i<MAX_USERS;i++)
{

if(users[i].active && strings_equal(users[i].username, username))
{

return &users[i];

}

}

return 0;

}



int user_set_password(
user* u,
char* password
)
{

if(u == 0)
{

return 0;

}

/*
    Meme logique que user_create() : nouveau sel derive du
    tick systeme courant et de l'id utilisateur, pour ne
    jamais reutiliser le sel precedent (deux mots de passe
    identiques avant/apres changement donneraient sinon le
    meme hash).
*/

u32 salt = (u32)timer_ticks() ^ (u->id * 2654435761u);

u->password_salt = salt;

u->password_hash = password_hash(password, salt);

return 1;

}



int user_delete(
char* username
)
{

user* u = user_find(username);

if(u == 0)
{

return 0;

}

/*
    id 1 = root (voir user_system_init()). Le supprimer
    laisserait le systeme sans aucun compte capable
    d'administrer les autres (creer/supprimer des
    utilisateurs, changer leur mot de passe...).
*/

if(u->id == 1)
{

return 0;

}

u->active = 0;

if(current_user == u)
{

current_user = 0;

}

return 1;

}



u32 user_slot_count()
{

return user_count;

}



user* user_get_slot(
u32 index
)
{

if(index >= user_count)
{

return 0;

}

return &users[index];

}
