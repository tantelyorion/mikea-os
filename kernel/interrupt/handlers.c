#include "handlers.h"

#include "pic.h"

#include "irq.h"

#include "../process/thread.h"

#include "../drivers/timer/timer.h"

#include "../drivers/keyboard/keyboard.h"

#include "../drivers/mouse/mouse.h"



/*
    Correctif (erreur d'execution sous QEMU) : isr_handler() se
    contentait de figer la machine (cli + hlt) sans RIEN afficher
    quand une exception CPU survenait (page fault, #GP, division
    par zero...). Pour l'utilisateur : un ecran fige ou deforme,
    sans aucune piste. On affiche maintenant un ecran d'erreur
    lisible (nom de l'exception, code d'erreur, RIP, CR2,
    registres principaux), en mode graphique comme en mode texte.

    Cette routine n'utilise NI le tas NI la console (qui
    pourraient etre la cause de la panne) : uniquement des
    tampons locaux et les primitives de dessin brutes.
*/

#include "../drivers/framebuffer.h"
#include "../drivers/graphics/graphics.h"

static const char* exception_name(u64 v)
{
    static const char* names[32] = {
        "Division par zero (#DE)", "Debogage (#DB)", "Interruption non masquable (NMI)",
        "Point d'arret (#BP)", "Debordement (#OF)", "Depassement de bornes (#BR)",
        "Instruction invalide (#UD)", "Coprocesseur absent (#NM)", "Double faute (#DF)",
        "Segment coprocesseur", "TSS invalide (#TS)", "Segment absent (#NP)",
        "Faute de pile (#SS)", "Protection generale (#GP)", "Faute de page (#PF)",
        "Reserve", "Erreur x87 (#MF)", "Alignement (#AC)", "Machine check (#MC)",
        "Erreur SIMD (#XM)", "Virtualisation (#VE)", "Reserve", "Reserve", "Reserve",
        "Reserve", "Reserve", "Reserve", "Reserve", "Reserve", "Reserve", "Securite (#SX)",
        "Reserve"
    };
    return (v < 32) ? names[v] : "Exception inconnue";
}

static void panic_hex(char* out, u64 value)
{
    static const char digits[] = "0123456789ABCDEF";
    out[0] = '0';
    out[1] = 'x';
    for (int i = 0; i < 16; i++)
    {
        out[2 + i] = digits[(value >> (60 - 4 * i)) & 0xF];
    }
    out[18] = 0;
}

static int panic_row = 0;

static void panic_line(const char* label, u64 value, int with_value)
{
    char line[64];
    int n = 0;
    while (label[n] != 0 && n < 40)
    {
        line[n] = label[n];
        n++;
    }
    if (with_value)
    {
        char hex[19];
        panic_hex(hex, value);
        for (int i = 0; hex[i] != 0; i++)
        {
            line[n++] = hex[i];
        }
    }
    line[n] = 0;

    if (gfx_available())
    {
        u32 x = 24;
        u32 y = 56 + (u32)panic_row * 20;
        gfx_draw_text(x, y, line, 0xFFFFFF, 1);
    }
    else
    {
        fb_write(line, 2, 3 + panic_row);
    }
    panic_row++;
}

void isr_handler(u64 vector, u64* frame)
{
    irq_disable();

    /*
        Double buffering (voir kernel/drivers/graphics) : le timer ne
        tournera plus apres une exception. On affiche donc tout ce qui est
        en attente puis on repasse en ecriture DIRECTE dans la memoire
        video, pour que le message de panique ci-dessous soit visible.
    */

    gfx_set_direct();

    u64 cr2 = 0;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));

    panic_row = 0;

    if (gfx_available())
    {
        gfx_fill_rect(0, 0, gfx_width(), gfx_height(), 0x8B1A1A);
        gfx_fill_rect(0, 0, gfx_width(), 40, 0x5A0F0F);
        gfx_draw_text(24, 12, "MIKEA OS - ERREUR NOYAU (exception CPU)", 0xFFFFFF, 2);
    }
    else
    {
        fb_clear();
        fb_write("MIKEA OS - ERREUR NOYAU (exception CPU)", 2, 1);
    }

    panic_line(exception_name(vector), 0, 0);
    panic_line("Vecteur      : ", vector, 1);
    panic_line("Code d'erreur: ", frame[16], 1);
    panic_line("RIP          : ", frame[17], 1);
    panic_line("CS           : ", frame[18], 1);
    panic_line("RFLAGS       : ", frame[19], 1);
    panic_line("CR2          : ", cr2, 1);
    panic_line("RAX          : ", frame[14], 1);
    panic_line("RBX          : ", frame[13], 1);
    panic_line("RCX          : ", frame[12], 1);
    panic_line("RDX          : ", frame[11], 1);
    panic_line("RSI          : ", frame[10], 1);
    panic_line("RDI          : ", frame[9], 1);
    panic_line("RBP          : ", frame[8], 1);
    panic_line("Systeme arrete. Redemarrez la machine virtuelle.", 0, 0);

    while (1)
    {
        asm volatile("hlt");
    }
}



void irq_handler(u64 vector)
{

u8 irq = (u8)(vector - 32);


if (irq == 0)
{

/* IRQ0 : PIT, notre horloge systeme. */

timer_tick();

}
else if (irq == 1)
{

/* IRQ1 : clavier PS/2. */

keyboard_handle_irq();

}
else if (irq == 12)
{

/* IRQ12 : souris PS/2. */

mouse_handle_irq();

}


/* Il faut TOUJOURS signaler la fin d'IRQ, sinon le PIC */
/* ne delivre plus jamais aucune interruption ensuite.  */

pic_send_eoi(irq);


if (irq == 0)
{

        /*
            Filet de securite du double buffering : si plus rien n'a ete
            dessine depuis un tick et que l'etat n'est pas affiche, on
            l'affiche (voir gfx_tick()). Garantit qu'un dessin n'est
            jamais "perdu" dans le tampon, meme si aucun code n'appelle
            gfx_present().
        */

        gfx_tick();

/*
    Preemption reelle : avant ce fichier, seul un thread
    qui appelait lui-meme thread_yield() rendait la main
    aux autres (cooperatif pur). Desormais, le timer force
    un point de bascule a chaque tick, meme si le thread en
    cours ne cede jamais volontairement la main.
*/

thread_yield();

}

}
