#include "rtl8139.h"
#include "../../cpu/io.h"
#include "../../../libc/string.h"

#define R_IDR0     0x00
#define R_TSD0     0x10
#define R_TSAD0    0x20
#define R_RBSTART  0x30
#define R_CR       0x37
#define R_CAPR     0x38
#define R_IMR      0x3C
#define R_ISR      0x3E
#define R_TCR      0x40
#define R_RCR      0x44
#define R_CONFIG1  0x52
#define R_BMSR     0x64

#define RX_BUF_SIZE 8192
#define RX_PAD      (16 + 1500 + 4)
#define TX_COUNT    4

static u8 rx_buffer[RX_BUF_SIZE + RX_PAD] __attribute__((aligned(16)));
static u8 tx_buffers[TX_COUNT][2048] __attribute__((aligned(16)));

static u16 io_base = 0;
static u8 mac_address[6];
static u32 rx_offset = 0;
static u32 tx_index = 0;
static int tx_used[TX_COUNT];
static int ready = 0;

int rtl8139_init(pci_device_t* d)
{
    ready = 0;

    if (d == (pci_device_t*)0)
    {
        return -1;
    }

    if ((d->bar[0] & 1u) == 0)
    {
        return -2; /* on attend un BAR en espace d'E/S */
    }

    io_base = (u16)(d->bar[0] & ~3u);

    if (io_base == 0)
    {
        return -3;
    }

    pci_enable_busmaster(d);

    outb((u16)(io_base + R_CONFIG1), 0x00); /* mise sous tension */

    outb((u16)(io_base + R_CR), 0x10);      /* reset logiciel */

    for (u32 spin = 0; spin < 1000000; spin++)
    {
        if ((inb((u16)(io_base + R_CR)) & 0x10) == 0)
        {
            break;
        }
    }

    for (int i = 0; i < 6; i++)
    {
        mac_address[i] = inb((u16)(io_base + R_IDR0 + i));
    }

    mk_memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_offset = 0;

    outl((u16)(io_base + R_RBSTART), (u32)(u64)rx_buffer);
    outw((u16)(io_base + R_IMR), 0x0000);    /* pas d'interruptions : polling */
    outw((u16)(io_base + R_ISR), 0xFFFF);

    /* AB | AM | APM | WRAP : diffusion, multidiffusion, notre MAC ; pas de bouclage du tampon. */
    outl((u16)(io_base + R_RCR), 0x8Eu);
    outl((u16)(io_base + R_TCR), 0x03000700u);

    outb((u16)(io_base + R_CR), 0x0C);       /* RE | TE */

    for (int i = 0; i < TX_COUNT; i++)
    {
        tx_used[i] = 0;
    }
    tx_index = 0;

    ready = 1;
    return 0;
}

int rtl8139_present()
{
    return ready;
}

int rtl8139_link_up()
{
    if (!ready)
    {
        return 0;
    }
    return (inw((u16)(io_base + R_BMSR)) & 0x0004u) != 0;
}

const u8* rtl8139_mac()
{
    return mac_address;
}

int rtl8139_send(const u8* frame, u16 length)
{
    if (!ready || length == 0 || length > 1518)
    {
        return -1;
    }

    u32 i = tx_index;

    if (tx_used[i])
    {
        /* Attendre la fin de l'envoi precedent sur ce descripteur (bit OWN = 0x2000). */
        int done = 0;
        for (u32 spin = 0; spin < 1000000; spin++)
        {
            if (inl((u16)(io_base + R_TSD0 + 4 * i)) & 0x2000u)
            {
                done = 1;
                break;
            }
        }
        if (!done)
        {
            return -2;
        }
    }

    mk_memset(tx_buffers[i], 0, 64);
    mk_memcpy(tx_buffers[i], frame, length);

    u32 send_length = length < 60 ? 60 : length;

    outl((u16)(io_base + R_TSAD0 + 4 * i), (u32)(u64)tx_buffers[i]);
    outl((u16)(io_base + R_TSD0 + 4 * i), send_length);

    tx_used[i] = 1;
    tx_index = (i + 1) % TX_COUNT;

    return 0;
}

int rtl8139_recv(u8* buffer, u16 max_length)
{
    if (!ready)
    {
        return 0;
    }

    if (inb((u16)(io_base + R_CR)) & 0x01)
    {
        return 0; /* tampon de reception vide */
    }

    u8* header = rx_buffer + rx_offset;
    u16 status = (u16)(header[0] | (header[1] << 8));
    u16 length = (u16)(header[2] | (header[3] << 8));

    int result = 0;

    if ((status & 0x0001u) && length >= 18 && length <= 1522)
    {
        u16 frame_length = (u16)(length - 4); /* sans le CRC */

        if (frame_length <= max_length)
        {
            mk_memcpy(buffer, header + 4, frame_length);
            result = frame_length;
        }
    }
    else
    {
        /* Etat incoherent : on repart d'un tampon propre. */
        outb((u16)(io_base + R_CR), 0x04);
        outb((u16)(io_base + R_CR), 0x0C);
        rx_offset = 0;
        outw((u16)(io_base + R_CAPR), (u16)(0 - 16));
        return 0;
    }

    rx_offset = (rx_offset + length + 4 + 3) & ~3u;

    if (rx_offset >= RX_BUF_SIZE)
    {
        rx_offset -= RX_BUF_SIZE;
    }

    outw((u16)(io_base + R_CAPR), (u16)(rx_offset - 16));
    outw((u16)(io_base + R_ISR), 0xFFFF);

    return result;
}
