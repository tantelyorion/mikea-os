#include "idt.h"



/*
    Format reel d'une entree IDT en mode long (64 bits) :
    16 octets, pas 8. L'ancienne version de ce fichier
    utilisait un descripteur au format 32 bits (8 octets,
    sans place pour les 32 bits de poids fort de l'adresse
    du gestionnaire), qui n'aurait jamais fonctionne en
    mode long : toute interruption aurait fait planter la
    machine (triple fault) au lieu d'appeler un gestionnaire.
*/

struct idt_entry
{

u16 base_low;

u16 selector;

u8  ist;

u8  flags;

u16 base_mid;

u32 base_high;

u32 reserved;

} __attribute__((packed));



struct idt_ptr
{

u16 limit;

u64 base;

} __attribute__((packed));



static struct idt_entry idt[256];

static struct idt_ptr idtp;



/*
    Gestionnaires d'interruption : implementes en
    assembleur dans kernel/cpu/isr_stubs.asm (32 exceptions
    CPU + 16 IRQ materielles remappees sur 32-47).
*/

extern void isr0();  extern void isr1();  extern void isr2();  extern void isr3();
extern void isr4();  extern void isr5();  extern void isr6();  extern void isr7();
extern void isr8();  extern void isr9();  extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();

extern void irq0();  extern void irq1();  extern void irq2();  extern void irq3();
extern void irq4();  extern void irq5();  extern void irq6();  extern void irq7();
extern void irq8();  extern void irq9();  extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();



void idt_set_gate(u8 vector, u64 handler, u16 selector, u8 flags)
{

idt[vector].base_low  = (u16)(handler & 0xFFFF);

idt[vector].base_mid   = (u16)((handler >> 16) & 0xFFFF);

idt[vector].base_high = (u32)((handler >> 32) & 0xFFFFFFFF);

idt[vector].selector = selector;

idt[vector].ist = 0;

idt[vector].flags = flags;

idt[vector].reserved = 0;

}



static void idt_load()
{

idtp.limit = (u16)(sizeof(idt) - 1);

idtp.base = (u64)&idt;

asm volatile("lidt %0" : : "m"(idtp));

}



void idt_init()
{

for (int i = 0; i < 256; i++)
{

idt_set_gate((u8)i, 0, 0, 0);

}


/*
    Selecteur 0x08 : segment de code noyau (voir gdt.c).
    Attributs 0x8E : present, anneau 0, "interrupt gate"
    64 bits (et non "trap gate") -- ce qui masque
    automatiquement les interruptions (IF=0) le temps de
    traiter le gestionnaire, evitant qu'une IRQ ne se
    reenclenche au milieu d'un changement de contexte.
*/

#define GATE(n, handler) idt_set_gate(n, (u64)handler, 0x08, 0x8E)

GATE(0,  isr0);  GATE(1,  isr1);  GATE(2,  isr2);  GATE(3,  isr3);
GATE(4,  isr4);  GATE(5,  isr5);  GATE(6,  isr6);  GATE(7,  isr7);
GATE(8,  isr8);  GATE(9,  isr9);  GATE(10, isr10); GATE(11, isr11);
GATE(12, isr12); GATE(13, isr13); GATE(14, isr14); GATE(15, isr15);
GATE(16, isr16); GATE(17, isr17); GATE(18, isr18); GATE(19, isr19);
GATE(20, isr20); GATE(21, isr21); GATE(22, isr22); GATE(23, isr23);
GATE(24, isr24); GATE(25, isr25); GATE(26, isr26); GATE(27, isr27);
GATE(28, isr28); GATE(29, isr29); GATE(30, isr30); GATE(31, isr31);

GATE(32, irq0);  GATE(33, irq1);  GATE(34, irq2);  GATE(35, irq3);
GATE(36, irq4);  GATE(37, irq5);  GATE(38, irq6);  GATE(39, irq7);
GATE(40, irq8);  GATE(41, irq9);  GATE(42, irq10); GATE(43, irq11);
GATE(44, irq12); GATE(45, irq13); GATE(46, irq14); GATE(47, irq15);

#undef GATE

idt_load();

}
