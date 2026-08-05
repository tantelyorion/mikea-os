/*
    Demo hote (terminal) du code REEL de Mikea OS :
    gui/gui.c et kernel/console/console.c sont compiles et
    executes sans aucune modification -- seul le pilote VGA
    materiel (kernel/drivers/framebuffer.c, qui ecrit a
    l'adresse physique 0xB8000) est remplace par
    mock_framebuffer.c, qui redirige les memes appels
    fb_put/fb_clear/fb_scroll vers un tampon 80x25 affiche
    ensuite dans le terminal avec les couleurs ANSI
    correspondantes.

    Objectif : montrer concretement que la logique de la GUI
    corrigee (shell/commands.c -> "gui", jusqu'ici jamais
    appelee) fonctionne, sans necessiter nasm / la toolchain
    croisee x86_64-elf / QEMU (absents de ce bac a sable).
*/

#include <stdio.h>
#include <unistd.h>
#include "console.h"
#include "gui.h"
#include "framebuffer.h"
#include "user.h"
#include "permission.h"

void fb_render_to_terminal(void);

static void pause_step(const char* msg) {
    fb_render_to_terminal();
    printf("\n%s\n", msg);
    fflush(stdout);
    sleep(2);
}

int main(void) {
    fb_clear();
    console_init();
    permission_init();
    user_system_init();

    console_write("================================\n");
    console_write("\n");
    console_write("          Mikea OS 0.2.1\n");
    console_write("\n");
    console_write("       Kernel Starting...\n");
    console_write("\n");
    console_write("System Components\n");
    console_write("-----------------\n");
    console_write("Framebuffer Driver   : READY\n");
    console_write("Console System       : READY\n");
    console_write("GUI Module           : READY\n");
    console_write("\n");
    console_write("Mikea OS Core Ready\n");
    console_write("Starting User Environment...\n");

    pause_step("[demo] Sequence de demarrage (console.c reel) -- passage a l'ecran de connexion (security/login.c) ...");

    /* Reproduction de security/login.c::login_prompt(), sans
       lecture clavier reelle (pas de terminal interactif ici) :
       on se connecte directement en "root" / "mikea", les
       identifiants par defaut crees par user_system_init(). */
    user* logged_in = user_login("root", "mikea");
    console_write("\nMikea OS login: root\n");
    console_write("Password: ********\n\n");

    pause_step("[demo] Connexion reussie (security/user.c::user_login(), code reel) -- passage a 'msh> gui' ...");

    /* Reproduction exacte de cmd_gui() dans shell/commands.c,
       version desormais consciente de la session ouverte
       ci-dessus (voir le correctif : la GUI et la session
       utilisateur etaient jusqu'ici deux mondes etanches). */
    console_clear();
    gui_draw_window(10, 4, 58, 14, "Mikea OS - Session", 0x1F);

    if (logged_in == 0) {
        gui_draw_text(13, 8, "Aucun utilisateur connecte.", 0x1F);
    } else {
        char line[48];
        const char* label = "Utilisateur : ";
        int i = 0;
        while (label[i] && i < 47) { line[i] = label[i]; i++; }
        int j = 0;
        while (logged_in->username[j] && i < 47) { line[i] = logged_in->username[j]; i++; j++; }
        line[i] = 0;
        gui_draw_text(13, 7, line, 0x1F);

        if (logged_in->id == 1) {
            gui_draw_text(13, 8, "Role : administrateur (root)", 0x1F);
        } else {
            gui_draw_text(13, 8, "Role : utilisateur standard", 0x1F);
        }

        gui_draw_text(13, 10, "Permissions :", 0x1F);
        gui_draw_text(15, 11,
            check_permission((int)logged_in->id, PERMISSION_READ) ? "Lecture    : oui" : "Lecture    : non", 0x1F);
        gui_draw_text(15, 12,
            check_permission((int)logged_in->id, PERMISSION_WRITE) ? "Ecriture   : oui" : "Ecriture   : non", 0x1F);
        gui_draw_text(15, 13,
            check_permission((int)logged_in->id, PERMISSION_EXEC) ? "Execution  : oui" : "Execution  : non", 0x1F);
    }

    gui_draw_text(13, 16, "Appuyez sur Entree pour revenir au shell...", 0x1F);

    fb_render_to_terminal();
    printf("\n[demo] Fenetre GUI dessinee par gui_draw_window()/gui_draw_text() (code reel), avec le statut de la session reelle (user_get_current()/check_permission(), memes fonctions que 'whoami').\n");

    return 0;
}
