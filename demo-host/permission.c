#include "permission.h"

#include "user.h"



/*
    Table de permissions indexee par id utilisateur (1 a
    MAX_USERS). L'indice 0 n'est pas utilise (les id
    utilisateur commencent a 1, voir user_create()).
*/

static int permission_table[MAX_USERS + 1];



void permission_init()
{

for (int i = 0; i <= MAX_USERS; i++)
{

permission_table[i] = 0;

}


/*
    L'utilisateur root (id 1, cree par user_system_init())
    recoit toutes les permissions par defaut. Les autres
    utilisateurs n'en ont aucune tant qu'elles ne leur sont
    pas explicitement accordees via permission_grant().
*/

permission_table[1] = PERMISSION_READ | PERMISSION_WRITE | PERMISSION_EXEC;

}



int check_permission(
int user,
int permission
)
{

if (user <= 0 || user > MAX_USERS)
{

return 0;

}


return (permission_table[user] & permission) == permission;

}



void permission_grant(
int user,
int permission
)
{

if (user <= 0 || user > MAX_USERS)
{

return;

}


permission_table[user] |= permission;

}
