#ifndef MIKEA_GUI_WINDOW_H
#define MIKEA_GUI_WINDOW_H


#include "../include/types.h"


/*
    ============================================================
    Mikea OS - Gestion de fenetres (multi-fenetrage + reduction)
    ============================================================

    Avant ce fichier, chaque application (calculatrice,
    explorateur...) etait un simple appel BLOQUANT depuis le
    thread du bureau (gui/desktop.c) : ouvrir une application
    figeait completement le bureau jusqu'a sa fermeture --
    impossible de rouvrir le Centre d'applications, impossible
    de "reduire" quoi que ce soit (rien ne continuait a exister
    une fois la fenetre fermee, aucun etat a restaurer), et une
    seule fenetre pouvait exister a la fois.

    Ce module fait tourner chaque application dans son PROPRE
    fil d'execution (voir kernel/process/thread.c, qui fournit
    deja un ordonnanceur round-robin preemptif -- rien de neuf a
    construire cote noyau). Ca permet un etat REELLEMENT
    preserve par fenetre (simples variables locales du thread de
    chaque application) et donc une reduction/restauration
    veritable, plus plusieurs fenetres "ouvertes" en meme temps.

    Limite assumee, par securite : ce systeme n'implemente PAS
    un compositeur de fenetres complet (pas de superposition
    avec zones cliquables partagees, pas de redimensionnement
    partiel). A la place, une regle simple et stricte : UNE SEULE
    fenetre (ou le bureau lui-meme) a le "jeton de focus" a un
    instant donne -- seule celle qui le detient a le droit de
    lire la souris/le clavier et de dessiner a l'ecran. Toutes
    les autres (fenetres reduites, ou en arriere-plan) attendent
    passivement dans gui_window_idle(), sans jamais toucher au
    framebuffer ni interroger la souris -- ce qui elimine tout
    risque de conflit d'affichage ou de double-reaction a un
    meme clic entre deux fenetres, sans avoir besoin du moindre
    verrou complexe.

    Consequence assumee : fermer une session (deconnexion) ne
    termine PAS de force les fenetres restees ouvertes en
    arriere-plan -- leur thread continue d'exister, simplement en
    attente indefinie d'un focus qu'il ne recevra plus jamais
    (l'ecran de connexion suivant ne les reactive pas). Terminer
    de force un thread depuis l'exterieur, en plein milieu d'une
    pile d'appels arbitraire, n'est pas sur sans un veritable
    support noyau pour cela (absent ici) -- le cout reel est
    negligeable (quelques threads inactifs, tres largement sous
    la limite MAX_THREAD de kernel/process/thread.h), donc ce
    compromis est accepte plutot que de risquer une instabilite.
    ============================================================
*/


#define GUI_MAX_WINDOWS 6


typedef struct
{

int in_use;

char title[32];

int minimized;

} gui_window_slot;


void gui_window_system_init();


/*
    A appeler UNIQUEMENT depuis le thread du bureau (voir
    gui/desktop.c) : cree un nouveau thread pour "entry" et lui
    donne immediatement le focus. Renvoie l'indice du nouveau
    slot (>=0), ou -1 en cas d'echec (table de fenetres pleine,
    ou memoire insuffisante pour la pile du thread -- voir
    thread_create(), kernel/process/thread.c).
*/

int gui_window_open(const char* title, void (*entry)());


/*
    A appeler par l'application elle-meme, tout au debut de sa
    fonction (avant sa boucle principale) : recupere le numero
    de slot qui vient de lui etre attribue par gui_window_open().
    Necessaire car thread_create() ne transmet aucun argument
    aux threads qu'il demarre (voir kernel/process/thread.h) --
    l'association thread -> slot est retrouvee via l'identifiant
    unique du thread courant (thread_current()), sans variable
    partagee "en transit" qui pourrait se faire ecraser par un
    lancement suivant avant d'avoir ete lue.
*/

int gui_window_claim_slot();


/*
    Renvoie 1 si "slot" detient le jeton de focus actuellement
    (l'application doit alors dessiner et lire la souris/le
    clavier ce tour-ci), 0 sinon (l'application ne doit RIEN
    dessiner ni lire ce tour-ci -- voir gui_window_idle()).
*/

int gui_window_has_focus(int slot);


/*
    A appeler par une application qui n'a pas (ou plus) le
    focus : cede volontairement le CPU au thread suivant plutot
    que d'attendre la prochaine preemption materielle -- evite
    de gaspiller du temps CPU en arriere-plan pour rien.
*/

void gui_window_idle();


/*
    Rend le focus au bureau sans terminer le thread de
    l'application (bouton "-" de la barre de titre) : son etat
    (variables locales) reste intact, prete a etre redessinee
    des que gui_window_focus() la rappelle.
*/

void gui_window_minimize(int slot);


/*
    Termine DEFINITIVEMENT cette fenetre (bouton de fermeture) :
    libere son slot et rend le focus au bureau. A appeler juste
    avant que la fonction de l'application ne se termine
    elle-meme (son thread devient alors THREAD_TERMINATED, voir
    thread_trampoline()).
*/

void gui_window_close(int slot);


/*
    A appeler UNIQUEMENT depuis le thread du bureau : donne le
    focus a "slot" (restaurer une fenetre reduite depuis sa
    ligne de barre des taches, ou depuis le Centre
    d'applications).
*/

void gui_window_focus(int slot);


/* Renvoie 1 si c'est le bureau lui-meme qui a le focus (aucune fenetre), 0 sinon. */

int gui_window_desktop_has_focus();


/*
    Acces en lecture a la table des fenetres ouvertes, pour que
    gui/desktop.c puisse construire la barre des taches. "slot"
    hors bornes ou inutilise renvoie 0 (nul).
*/

const gui_window_slot* gui_window_get(int slot);


#endif
