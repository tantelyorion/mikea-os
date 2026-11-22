#include "serial.h"
#include "../../cpu/io.h"

#define COM1 0x3F8

unsigned long timer_ticks();

void serial_init()
{
    outb(COM1 + 1, 0x00);   /* pas d'interruptions */
    outb(COM1 + 3, 0x80);   /* DLAB on */
    outb(COM1 + 0, 0x01);   /* 115200 bauds */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   /* 8 bits, pas de parite, 1 stop */
    outb(COM1 + 2, 0xC7);   /* FIFO */
    outb(COM1 + 4, 0x0B);   /* DTR, RTS, OUT2 */
}

void serial_putc(char c)
{
    for (u32 spin = 0; spin < 200000; spin++)
    {
        if (inb(COM1 + 5) & 0x20)
        {
            break;
        }
    }
    outb(COM1, (u8)c);
}

void serial_write(const char* text)
{
    while (*text)
    {
        serial_putc(*text);
        text++;
    }
}

int serial_getc_timeout(u32 timeout_ticks)
{
    unsigned long start = timer_ticks();
    u64 spins = 0;
    u64 spin_limit = (u64)timeout_ticks * 300000ULL + 1000ULL;

    while (1)
    {
        if (inb(COM1 + 5) & 0x01)
        {
            return (int)inb(COM1);
        }

        if (timer_ticks() - start >= timeout_ticks)
        {
            return -1;
        }

        /* Garde-fou si le minuteur n'avance pas (interruptions coupees). */
        if (++spins > spin_limit)
        {
            return -1;
        }

        asm volatile("pause");
    }
}
