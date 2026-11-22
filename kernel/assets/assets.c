#include "assets.h"
#include "../memory/heap.h"
#include "../../boot/installer/installer.h"
#include "../../libc/string.h"

#define PAK_MAGIC       0x50414B4Du   /* "MKAP" en little-endian */
#define PAK_VERSION     1
#define PAK_MAX_ENTRIES 15
#define PAK_NAME_LEN    16
#define PAK_MAX_SECTORS (2048 - ASSETS_PAK_LBA)

typedef struct
{
    char name[PAK_NAME_LEN];
    u32  sector;    /* relatif au debut du pack */
    u32  size;
    u16  width;
    u16  height;
    u32  reserved;
} __attribute__((packed)) pak_entry;

typedef struct
{
    u32 magic;
    u32 version;
    u32 count;
    u32 base_lba;
    pak_entry entries[PAK_MAX_ENTRIES];
} __attribute__((packed)) pak_header;

static pak_header g_pak;
static int g_state = 0;   /* 0 = pas encore lu, 1 = valide, -1 = absent/invalide */

static void assets_init_once()
{
    u8 sector[512];

    if (g_state != 0)
    {
        return;
    }

    g_state = -1;

    if (installer_read_boot_sector(ASSETS_PAK_LBA, sector) != 0)
    {
        return;
    }

    mk_memcpy(&g_pak, sector, sizeof(g_pak));

    if (g_pak.magic != PAK_MAGIC || g_pak.version != PAK_VERSION
        || g_pak.count == 0 || g_pak.count > PAK_MAX_ENTRIES
        || g_pak.base_lba != ASSETS_PAK_LBA)
    {
        return;
    }

    /* Chaque entree doit tenir dans la zone reservee avant la fin de l'image. */

    for (u32 i = 0; i < g_pak.count; i++)
    {
        const pak_entry* e = &g_pak.entries[i];
        u32 sectors = (e->size + 511) / 512;

        if (e->size == 0 || e->sector < 1 || e->sector + sectors > PAK_MAX_SECTORS)
        {
            return;
        }
    }

    g_state = 1;
}

static const pak_entry* assets_lookup(const char* name)
{
    assets_init_once();

    if (g_state != 1)
    {
        return (const pak_entry*)0;
    }

    for (u32 i = 0; i < g_pak.count; i++)
    {
        if (mk_strncmp(g_pak.entries[i].name, name, PAK_NAME_LEN) == 0)
        {
            return &g_pak.entries[i];
        }
    }

    return (const pak_entry*)0;
}

int assets_find(const char* name, asset_info* info)
{
    const pak_entry* e = assets_lookup(name);

    if (e == (const pak_entry*)0)
    {
        return 0;
    }

    if (info != (asset_info*)0)
    {
        info->size = e->size;
        info->width = e->width;
        info->height = e->height;
    }

    return 1;
}

u8* assets_load(const char* name, u32* size_out)
{
    const pak_entry* e = assets_lookup(name);

    if (e == (const pak_entry*)0)
    {
        return (u8*)0;
    }

    u8* data = (u8*)mk_malloc(e->size);

    if (data == (u8*)0)
    {
        return (u8*)0;
    }

    u8 sector[512];
    u32 remaining = e->size;
    u32 done = 0;
    u32 lba = ASSETS_PAK_LBA + e->sector;

    while (remaining > 0)
    {
        u32 chunk = remaining < 512 ? remaining : 512;

        if (installer_read_boot_sector(lba, sector) != 0)
        {
            mk_free(data);
            return (u8*)0;
        }

        mk_memcpy(data + done, sector, chunk);

        done += chunk;
        remaining -= chunk;
        lba++;
    }

    if (size_out != (u32*)0)
    {
        *size_out = e->size;
    }

    return data;
}
