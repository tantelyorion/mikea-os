#include "pic.h"

#include "../cpu/io.h"



#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21

#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1


#define PIC_EOI      0x20


#define ICW1_ICW4      0x01
#define ICW1_INIT      0x10

#define ICW4_8086      0x01



void pic_remap()
{

/* Sauvegarde des masques d'interruption actuels. */

u8 mask1 = inb(PIC1_DATA);

u8 mask2 = inb(PIC2_DATA);


/* ICW1 : demarre la sequence d'initialisation. */

outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);

io_wait();

outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

io_wait();


/* ICW2 : offset des vecteurs (32 pour le maitre, 40 pour l'esclave). */

outb(PIC1_DATA, 32);

io_wait();

outb(PIC2_DATA, 40);

io_wait();


/* ICW3 : cascade maitre/esclave (IRQ2 relie les deux PIC). */

outb(PIC1_DATA, 0x04);

io_wait();

outb(PIC2_DATA, 0x02);

io_wait();


/* ICW4 : mode 8086. */

outb(PIC1_DATA, ICW4_8086);

io_wait();

outb(PIC2_DATA, ICW4_8086);

io_wait();


/* Restaure les masques d'origine. */

outb(PIC1_DATA, mask1);

outb(PIC2_DATA, mask2);

}



void pic_send_eoi(u8 irq)
{

if (irq >= 8)
{

outb(PIC2_COMMAND, PIC_EOI);

}


outb(PIC1_COMMAND, PIC_EOI);

}
