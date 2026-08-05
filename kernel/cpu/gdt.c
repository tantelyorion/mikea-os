#include "gdt.h"



/*
    Correctif majeur : ce fichier construisait bien un
    tableau "gdt" en memoire (3 descripteurs), mais ne
    l'installait JAMAIS dans le CPU. Il manquait :

      1. Le pointeur GDT (limite + adresse) et l'instruction
         LGDT pour charger cette table.
      2. La reinitialisation des registres de segment (CS via
         un saut lointain, DS/ES/FS/GS/SS directement) pour
         que le CPU utilise reellement les nouveaux
         descripteurs au lieu de ceux herites du chargeur
         (boot/loader/stage2.asm).
      3. Des attributs corrects pour le segment de code en
         mode long 64 bits : l'octet "granularity" du
         segment code valait 0xCF (bit L=0, D/B=1), ce qui
         decrit un segment 32 bits -- incoherent avec un
         noyau qui s'execute deja en mode long 64 bits.
         Il doit valoir 0xAF (L=1, D/B=0) pour un vrai
         segment de code 64 bits.

    Sans LGDT, gdt_init() n'avait donc strictement aucun
    effet : le CPU continuait a utiliser la GDT du chargeur.
*/


struct gdt_entry
{

unsigned short limit_low;

unsigned short base_low;

unsigned char base_middle;

unsigned char access;

unsigned char granularity;

unsigned char base_high;

} __attribute__((packed));



struct gdt_ptr
{

unsigned short limit;

unsigned long base;

} __attribute__((packed));



static struct gdt_entry gdt[3];

static struct gdt_ptr gdtp;



static void gdt_set_entry(
int index,
unsigned char access,
unsigned char granularity
)
{

/*
    Base = 0, limite = maximale : en mode long, la
    segmentation est essentiellement ignoree pour le code
    et les donnees (modele "flat"), seuls comptent les bits
    d'attributs (present, ring, executable, long mode...).
*/

gdt[index].limit_low = 0xFFFF;

gdt[index].base_low = 0;

gdt[index].base_middle = 0;

gdt[index].access = access;

gdt[index].granularity = granularity;

gdt[index].base_high = 0;

}



void gdt_init()
{


/*
Entry 0 :
NULL descriptor
*/

gdt[0].limit_low=0;

gdt[0].base_low=0;

gdt[0].base_middle=0;

gdt[0].access=0;

gdt[0].granularity=0;

gdt[0].base_high=0;



/*
    Entry 1 : Code Segment, anneau 0, mode long 64 bits.
    access 0x9A  = present, ring 0, code, executable, lisible
    granularity 0xAF = G=1, D/B=0, L=1 (64 bits), limit=0xF
*/

gdt_set_entry(1, 0x9A, 0xAF);



/*
    Entry 2 : Data Segment, anneau 0.
    access 0x92 = present, ring 0, donnees, inscriptible
*/

gdt_set_entry(2, 0x92, 0xCF);



gdtp.limit = (unsigned short)(sizeof(gdt) - 1);

gdtp.base = (unsigned long)&gdt;


asm volatile("lgdt %0" : : "m"(gdtp));


/*
    Recharge tous les registres de segment sur les nouveaux
    descripteurs : DS/ES/FS/GS/SS directement (selecteur
    0x10 = entree 2 * 8), et CS via un saut lointain vers
    l'instruction suivante (selecteur 0x08 = entree 1 * 8),
    seule methode pour changer CS sur x86_64.
*/

asm volatile(
"mov $0x10, %%ax\n\t"
"mov %%ax, %%ds\n\t"
"mov %%ax, %%es\n\t"
"mov %%ax, %%fs\n\t"
"mov %%ax, %%gs\n\t"
"mov %%ax, %%ss\n\t"
"pushq $0x08\n\t"
"lea 1f(%%rip), %%rax\n\t"
"pushq %%rax\n\t"
"lretq\n\t"
"1:\n\t"
:
:
: "rax"
);


}
