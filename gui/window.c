#include "window.h"

#include "../kernel/process/thread.h"

#include "../kernel/interrupt/irq.h"

#include "../libc/string.h"


static gui_window_slot g_windows[GUI_MAX_WINDOWS];


/*
    -1 = le bureau lui-meme a le focus. Sinon, indice dans
    g_windows[] de la fenetre qui a le focus. Un seul ecrivain a
    la fois (voir le commentaire de window.h) : la fenetre/le
    bureau qui detient actuellement le focus est la seule partie
    autorisee a le modifier (en le rendant), ou le thread du
    bureau lorsqu'il en attribue un nouveau -- jamais deux
    ecritures concurrentes possibles par construction.
*/

static int g_focused_slot = -1;


/*
    Associe l'identifiant unique d'un thread (thread->id,
    attribue sequentiellement par thread_create(), voir
    kernel/process/thread.c) au slot de fenetre qui lui a ete
    attribue -- evite toute variable "en transit" partagee entre
    gui_window_open() et gui_window_claim_slot() qui pourrait se
    faire ecraser par un lancement suivant avant d'avoir ete lue
    par le nouveau thread.
*/

static int g_window_of_thread[MAX_THREAD + 1];


void gui_window_system_init()
{

for (int i = 0; i < GUI_MAX_WINDOWS; i++)
{

g_windows[i].in_use = 0;

g_windows[i].minimized = 0;

g_windows[i].title[0] = 0;

}


for (int i = 0; i <= MAX_THREAD; i++)
{

g_window_of_thread[i] = -1;

}


g_focused_slot = -1;

}


int gui_window_open(const char* title, void (*entry)())
{


int slot = -1;

for (int i = 0; i < GUI_MAX_WINDOWS; i++)
{

if (!g_windows[i].in_use)
{

slot = i;

break;

}

}


if (slot < 0)
{

return -1;

}


/*
    Section critique : entre la creation du thread et
    l'enregistrement de son slot dans g_window_of_thread[], une
    preemption materielle qui laisserait ce tout nouveau thread
    s'executer avant que l'association ne soit ecrite lui ferait
    lire un slot invalide (-1) dans gui_window_claim_slot().
    irq_disable()/irq_enable() (kernel/interrupt/irq.c) sont
    deja utilises ailleurs dans ce noyau pour la meme raison
    (voir kernel/interrupt/handlers.c).
*/

irq_disable();


g_windows[slot].in_use = 1;

g_windows[slot].minimized = 0;

mk_strcpy(g_windows[slot].title, title, sizeof(g_windows[slot].title));


thread* t = thread_create(entry);


if (t == 0)
{

g_windows[slot].in_use = 0;

irq_enable();

return -1;

}


if (t->id <= MAX_THREAD)
{

g_window_of_thread[t->id] = slot;

}


g_focused_slot = slot;


irq_enable();


return slot;


}


int gui_window_claim_slot()
{

thread* self = thread_current();


if (self == 0 || self->id > MAX_THREAD)
{

return -1;

}


return g_window_of_thread[self->id];

}


int gui_window_has_focus(int slot)
{

return slot >= 0 && slot == g_focused_slot;

}


void gui_window_idle()
{

thread_yield();

}


void gui_window_minimize(int slot)
{

if (slot < 0 || slot >= GUI_MAX_WINDOWS || !g_windows[slot].in_use)
{

return;

}


g_windows[slot].minimized = 1;

g_focused_slot = -1;

}


void gui_window_close(int slot)
{

if (slot < 0 || slot >= GUI_MAX_WINDOWS)
{

return;

}


g_windows[slot].in_use = 0;

g_windows[slot].minimized = 0;

g_focused_slot = -1;

}


void gui_window_focus(int slot)
{

if (slot < 0 || slot >= GUI_MAX_WINDOWS || !g_windows[slot].in_use)
{

return;

}


g_windows[slot].minimized = 0;

g_focused_slot = slot;

}


int gui_window_desktop_has_focus()
{

return g_focused_slot == -1;

}


const gui_window_slot* gui_window_get(int slot)
{

if (slot < 0 || slot >= GUI_MAX_WINDOWS || !g_windows[slot].in_use)
{

return (void*)0;

}


return &g_windows[slot];

}
