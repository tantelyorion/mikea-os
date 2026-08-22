#ifndef MIKEA_GUI_H
#define MIKEA_GUI_H


#include "../include/types.h"


/*
    ============================================================
    Mikea OS - GUI
    ============================================================

    Bascule automatiquement entre deux rendus :
    - Mode texte (defaut historique) : caracteres/couleurs VGA
      via kernel/drivers/framebuffer.c.
    - Mode graphique pixels (des que gfx_available() est vrai,
      voir kernel/drivers/graphics/) : fenetre reelle avec
      bordure, fond distinct et police bitmap agrandie.

    Etape 5 (interaction souris <-> fenetre) : bouton de
    fermeture cliquable et curseur souris, mode graphique
    uniquement -- sans effet en mode texte (pas de souris a
    afficher dans un terminal texte classique).
    ============================================================
*/


/*
    Dessine une boite avec bordure a la position (x, y),
    de largeur "width" et hauteur "height" (en caracteres).
*/

void gui_draw_box(int x, int y, int width, int height, u8 color);


/*
    Dessine une "fenetre" : une boite avec une barre de
    titre sur sa premiere ligne. En mode graphique, dessine
    aussi trois boutons dans le coin superieur droit de la
    barre de titre -- fermer ('X', accent rouge), agrandir/
    restaurer (case(s), gris) et reduire ('-', gris) -- style
    "feux de circulation" macOS/Windows/GNOME, et renseigne
    leurs coordonnees pixel pour un test de collision avec la
    souris -- voir gui_point_in_button(). "is_maximized"
    determine l'icone du bouton agrandir (case pleine =
    agrandir, deux cases superposees = restaurer, meme
    convention que Windows). Les pointeurs de sortie peuvent
    etre nuls si l'appelant n'en a pas besoin (ex. mode texte,
    ou pas de bouton reduire/agrandir souhaite -- voir
    gui/gui.c, gui_text_prompt()).
*/

void gui_draw_window(
int x, int y, int width, int height, const char* title, u8 color,
u32* bx, u32* by, u32* bsize,
u32* max_bx, u32* max_by, u32* max_bsize, int is_maximized,
u32* min_bx, u32* min_by, u32* min_bsize
);


/*
    Ecrit du texte a une position donnee, sans dependre du
    curseur "fil de l'eau" de kernel/console (utile a
    l'interieur d'une fenetre).
*/

void gui_draw_text(int x, int y, const char* text, u8 color);


/*
    Dessine un curseur souris simple a la position pixel
    donnee. Sans effet en mode texte.
*/

void gui_draw_cursor(s32 x, s32 y);


/*
    Restaure le fond sous la derniere position dessinee du
    curseur, sans rien dessiner de nouveau. A appeler avant de
    quitter une boucle interactive utilisant gui_draw_cursor(),
    pour ne pas laisser de trace visible.
*/

void gui_cursor_erase();


/*
    Correctif (residus visuels du curseur en changeant
    d'ecran) : contrairement a gui_cursor_erase(), qui
    RESTAURE les pixels sauvegardes avant d'oublier le
    curseur, cette fonction se contente d'oublier -- sans rien
    redessiner. A appeler juste apres avoir efface tout l'ecran
    par un autre moyen (ex. console_clear() -> gfx_clear()) :
    dans ce cas, le tampon sauvegarde par gui_draw_cursor() ne
    correspond plus a rien de valide sur le nouvel ecran, et le
    restaurer y peindrait un bloc de pixels perimes ("fantome"
    du curseur a son ancienne position). Sans effet si aucun
    curseur n'etait affiche.
*/

void gui_cursor_reset();


/*
    Teste si le point pixel (px, py) se trouve dans un bouton
    carre de cote "bsize" dont le coin superieur gauche est
    (bx, by) -- voir les coordonnees renvoyees par
    gui_draw_window().
*/

int gui_point_in_button(u32 bx, u32 by, u32 bsize, s32 px, s32 py);


/*
    Version generique (rectangle, pas seulement carre) du test
    de collision ci-dessus -- utilisee par les boutons
    d'application (ex. calculatrice) qui ne sont pas forcement
    carres.
*/

int gui_point_in_rect(u32 rx, u32 ry, u32 rw, u32 rh, s32 px, s32 py);


/*
    Dessine un bouton rectangulaire generique (bordure +
    libelle centre approximativement), en caracteres (x, y,
    width, height). Renvoie ses coordonnees pixel via
    out_x/out_y/out_w/out_h (peuvent etre nuls), pour un test
    de collision avec gui_point_in_rect(). Sans effet en mode
    texte (les applications interactives -- calculatrice etc. --
    necessitent une souris, donc le mode graphique).
*/

void gui_draw_button(int x, int y, int w, int h, const char* label, u32* out_x, u32* out_y, u32* out_w, u32* out_h);


/*
    Champ de saisie cliquable (ex. ecran de connexion, voir
    gui/login_screen.c) : boite avec bordure, texte affiche a
    l'interieur (ou des etoiles si "masked" est vrai -- mot de
    passe), bordure plus marquee quand "focused" est vrai pour
    indiquer que la frappe clavier ira dans ce champ (meme
    convention visuelle que les "focus rings" habituels de
    Windows/macOS/GNOME). "text" n'est jamais modifie par cette
    fonction : c'est a l'appelant de gerer la frappe clavier et
    de rappeler cette fonction avec le contenu mis a jour.
    Graphique uniquement (aucun effet en mode texte, comme
    gui_draw_button()).
*/

void gui_draw_field(int x, int y, int w, int h, const char* text, int masked, int focused, u32* out_x, u32* out_y, u32* out_w, u32* out_h);


/*
    ============================================================
    Fenetres deplacables (glisser-deposer par la barre de titre)
    ============================================================

    Auparavant, chaque fenetre (calculatrice, explorateur de
    fichiers, parametres, compte) restait figee a sa position
    de depart pendant toute la duree de l'application -- aucune
    des applications GUI existantes ne suivait la souris pour
    se deplacer, contrairement a n'importe quel bureau habituel
    (Windows/macOS/GNOME). Cette petite structure d'etat +
    fonction de mise a jour ajoutent ce comportement sans
    dupliquer sa logique dans chaque application : chacune n'a
    qu'a declarer une "gui_drag" locale et appeler
    gui_drag_update() a chaque tour de sa boucle, juste apres
    avoir dessine sa fenetre avec gui_draw_window().

    Limitation assumee : le deplacement se fait par pas de 16
    pixels (une "cellule" de texte, voir GFX_SCALE) plutot que
    pixel par pixel -- gui_draw_window()/gui_draw_box() ne
    prennent que des coordonnees en cellules de texte, pas en
    pixels, et changer cette convention toucherait toutes les
    applications existantes pour un gain surtout cosmetique. Le
    deplacement reste fluide a l'oeil malgre ce pas.
*/

typedef struct
{

int active;

s32 grab_offset_x;

s32 grab_offset_y;

} gui_drag;


/*
    A appeler a chaque tour de boucle, apres avoir dessine la
    fenetre. "win_x"/"win_y" (cellules de texte) sont mis a
    jour directement si un glisser est en cours. "width"/
    "height" (cellules) servent a borner la fenetre a l'ecran
    (on ne peut pas la faire glisser hors de l'ecran). "close_bx"/
    "close_by"/"close_bsize" (coordonnees pixel renvoyees par
    gui_draw_window()) delimitent le bouton de fermeture, exclu
    de la zone de saisie du glisser (cliquer dessus doit
    fermer la fenetre, pas la deplacer). Renvoie 1 si la
    position a change ce tour-ci (l'appelant doit alors
    redessiner), 0 sinon.
*/

int gui_drag_update(
gui_drag* drag,
int* win_x, int* win_y,
int width, int height,
s32 mouse_x, s32 mouse_y, int mouse_down,
u32 close_bx, u32 close_by, u32 close_bsize
);


/*
    Boite de dialogue de saisie modale (renommer un fichier,
    nommer un nouveau fichier/dossier...) : petit panneau avec
    un champ de texte (deja pre-rempli avec "initial_value",
    utile pour "renommer" -- vide pour "nouveau fichier") et
    deux boutons "OK"/"Annuler". Bloque jusqu'a ce que
    l'utilisateur choisisse l'un des deux (clic ou Entree/
    Echap -- ce clavier n'a pas de touche Echap distincte,
    voir kernel/drivers/keyboard, donc Entree valide toujours;
    seul le clic sur "Annuler" annule). Ecrit le resultat dans
    "buffer" (deja alloue par l'appelant, "max_len" octets) et
    renvoie 1 si "OK" a ete choisi avec un texte non vide, 0
    si "Annuler" a ete choisi (buffer laisse intact). Graphique
    uniquement (appelant responsable de ne pas l'invoquer hors
    mode graphique, comme le reste de gui.h).
*/

int gui_text_prompt(const char* title, const char* label, const char* initial_value, char* buffer, int max_len);


#endif
