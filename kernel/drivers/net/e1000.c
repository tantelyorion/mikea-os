#include "e1000.h"
#include "../../../libc/string.h"

/* Registres (offsets MMIO depuis BAR0). */
#define REG_CTRL   0x0000
#define REG_STATUS 0x0008
#define REG_ICR    0x00C0
#define REG_IMC    0x00D8
#define REG_RCTL   0x0100
#define REG_TCTL   0x0400
#define REG_TIPG   0x0410
#define REG_RDBAL  0x2800
#define REG_RDBAH  0x2804
#define REG_RDLEN  0x2808
#define REG_RDH    0x2810
#define REG_RDT    0x2818
#define REG_TDBAL  0x3800
#define REG_TDBAH  0x3804
#define REG_TDLEN  0x3808
#define REG_TDH    0x3810
#define REG_TDT    0x3818
#define REG_MTA    0x5200
#define REG_RAL0   0x5400
#define REG_RAH0   0x5404

#define CTRL_SLU   (1u << 6)
#define CTRL_ASDE  (1u << 5)
#define CTRL_LRST  (1u << 3)
#define CTRL_RST   (1u << 26)
#define CTRL_PHY_RST (1u << 31)

#define RCTL_EN    (1u << 1)
#define RCTL_BAM   (1u << 15)
#define RCTL_SECRC (1u << 26)

#define TCTL_EN    (1u << 1)
#define TCTL_PSP   (1u << 3)

#define RX_COUNT 32
#define TX_COUNT 8
#define BUF_SIZE 2048

typedef struct
{
    u64 addr;
    u16 length;
    u16 checksum;
    u8  status;
    u8  errors;
    u16 special;
} __attribute__((packed)) rx_desc_t;

typedef struct
{
    u64 addr;
    u16 length;
    u8  cso;
    u8  cmd;
    u8  status;
    u8  css;
    u16 special;
} __attribute__((packed)) tx_desc_t;

/*
    Anneaux et tampons DMA : statiques et alignes. Le noyau est
    identity-mappe (adresse virtuelle == adresse physique, sous
    4 Go), la carte peut donc recevoir ces adresses telles quelles.
*/
static rx_desc_t rx_ring[RX_COUNT] __attribute__((aligned(128)));
static tx_desc_t tx_ring[TX_COUNT] __attribute__((aligned(128)));
static u8 rx_buffers[RX_COUNT][BUF_SIZE] __attribute__((aligned(16)));
static u8 tx_buffers[TX_COUNT][BUF_SIZE] __attribute__((aligned(16)));

static volatile u8* mmio = (volatile u8*)0;
static u8 mac_address[6];
static u32 rx_current = 0;
static u32 tx_tail = 0;
static int ready = 0;

static u32 reg_read(u32 reg)
{
    return *(volatile u32*)(mmio + reg);
}

static void reg_write(u32 reg, u32 value)
{
    *(volatile u32*)(mmio + reg) = value;
}

int e1000_init(pci_device_t* d)
{
    ready = 0;

    if (d == (pci_device_t*)0)
    {
        return -1;
    }

    u32 bar0 = d->bar[0];

    if (bar0 & 1u)
    {
        return -2; /* BAR en espace E/S : variante non geree. */
    }

    u64 base = (u64)(bar0 & ~0xFu);

    if (((bar0 >> 1) & 3u) == 2u)
    {
        base |= ((u64)d->bar[1]) << 32;
    }

    /* Seuls les 4 premiers Go sont mappes par stage2. */
    if (base == 0 || base >= 0x100000000ULL)
    {
        return -3;
    }

    pci_enable_busmaster(d);

    mmio = (volatile u8*)base;

    reg_write(REG_IMC, 0xFFFFFFFFu);
    (void)reg_read(REG_ICR);

    reg_write(REG_CTRL, reg_read(REG_CTRL) | CTRL_RST);

    for (u32 spin = 0; spin < 1000000; spin++)
    {
        if ((reg_read(REG_CTRL) & CTRL_RST) == 0)
        {
            break;
        }
    }

    reg_write(REG_IMC, 0xFFFFFFFFu);
    (void)reg_read(REG_ICR);

    u32 ctrl = reg_read(REG_CTRL);
    ctrl |= CTRL_SLU | CTRL_ASDE;
    ctrl &= ~(CTRL_LRST | CTRL_PHY_RST);
    reg_write(REG_CTRL, ctrl);

    u32 ral = reg_read(REG_RAL0);
    u32 rah = reg_read(REG_RAH0);

    mac_address[0] = (u8)(ral & 0xFF);
    mac_address[1] = (u8)((ral >> 8) & 0xFF);
    mac_address[2] = (u8)((ral >> 16) & 0xFF);
    mac_address[3] = (u8)((ral >> 24) & 0xFF);
    mac_address[4] = (u8)(rah & 0xFF);
    mac_address[5] = (u8)((rah >> 8) & 0xFF);

    if (ral == 0 && (rah & 0xFFFF) == 0)
    {
        /* MAC par defaut de QEMU si la carte n'en fournit pas. */
        const u8 fallback[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
        mk_memcpy(mac_address, fallback, 6);
        reg_write(REG_RAL0, 0x12005452u);
        rah = 0x5634u;
    }

    reg_write(REG_RAH0, (rah & 0xFFFFu) | (1u << 31));

    for (u32 i = 0; i < 128; i++)
    {
        reg_write(REG_MTA + i * 4, 0);
    }

    /* --- Reception --- */
    for (u32 i = 0; i < RX_COUNT; i++)
    {
        rx_ring[i].addr = (u64)(&rx_buffers[i][0]);
        rx_ring[i].status = 0;
        rx_ring[i].length = 0;
    }

    reg_write(REG_RDBAL, (u32)(u64)rx_ring);
    reg_write(REG_RDBAH, 0);
    reg_write(REG_RDLEN, RX_COUNT * (u32)sizeof(rx_desc_t));
    reg_write(REG_RDH, 0);
    reg_write(REG_RDT, RX_COUNT - 1);
    rx_current = 0;

    reg_write(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC);

    /* --- Emission --- */
    for (u32 i = 0; i < TX_COUNT; i++)
    {
        tx_ring[i].addr = (u64)(&tx_buffers[i][0]);
        tx_ring[i].cmd = 0;
        tx_ring[i].status = 1; /* DD : descripteur libre */
    }

    reg_write(REG_TDBAL, (u32)(u64)tx_ring);
    reg_write(REG_TDBAH, 0);
    reg_write(REG_TDLEN, TX_COUNT * (u32)sizeof(tx_desc_t));
    reg_write(REG_TDH, 0);
    reg_write(REG_TDT, 0);
    tx_tail = 0;

    reg_write(REG_TCTL, TCTL_EN | TCTL_PSP | (0x0Fu << 4) | (0x40u << 12));
    reg_write(REG_TIPG, 0x0060200Au);

    ready = 1;
    return 0;
}

int e1000_present()
{
    return ready;
}

int e1000_link_up()
{
    if (!ready)
    {
        return 0;
    }
    return (reg_read(REG_STATUS) & 0x2u) != 0;
}

const u8* e1000_mac()
{
    return mac_address;
}

int e1000_send(const u8* frame, u16 length)
{
    if (!ready || length == 0 || length > E1000_MAX_FRAME)
    {
        return -1;
    }

    u32 index = tx_tail;
    volatile tx_desc_t* desc = (volatile tx_desc_t*)&tx_ring[index];

    /* Attendre que ce descripteur soit libre (borne : jamais bloquant). */
    for (u32 spin = 0; spin < 1000000; spin++)
    {
        if (desc->status & 1u)
        {
            break;
        }
    }

    if (!(desc->status & 1u))
    {
        return -2;
    }

    mk_memcpy(tx_buffers[index], frame, length);

    desc->length = length;
    desc->cmd = 0x0B; /* EOP | IFCS | RS */
    desc->status = 0;

    tx_tail = (index + 1) % TX_COUNT;
    reg_write(REG_TDT, tx_tail);

    return 0;
}

int e1000_recv(u8* buffer, u16 max_length)
{
    if (!ready)
    {
        return 0;
    }

    volatile rx_desc_t* desc = (volatile rx_desc_t*)&rx_ring[rx_current];

    if (!(desc->status & 1u))
    {
        return 0;
    }

    u16 length = desc->length;
    int result = 0;

    if (length > 0 && length <= max_length && (desc->errors == 0))
    {
        mk_memcpy(buffer, rx_buffers[rx_current], length);
        result = length;
    }

    desc->status = 0;
    reg_write(REG_RDT, rx_current);
    rx_current = (rx_current + 1) % RX_COUNT;

    /* Trame invalide/trop grande : on la jette mais on signale "rien". */
    return result;
}
