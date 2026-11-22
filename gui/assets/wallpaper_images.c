#include "wallpaper_images.h"
#include "../../kernel/assets/assets.h"
#include "../../kernel/memory/heap.h"

typedef struct
{
    const char* display_name;
    const char* asset_name;
} wallpaper_entry;

/* L'ordre est celui des indices utilises par apps/settings et gui/desktop. */

static const wallpaper_entry WALLPAPERS[] = {
    { "Brume et collines (N&B)", "wp_brume" },
    { "Collines verdoyantes",    "wp_collines" },
    { "Cameleon",                "wp_cameleon" },
    { "Lune",                    "wp_lune" },
    { "Nebuleuse d'Orion",       "wp_nebuleuse" },
    { "Vaisseau SpaceX",         "wp_satellite" },
};

#define WALLPAPER_COUNT ((u32)(sizeof(WALLPAPERS) / sizeof(WALLPAPERS[0])))

static wallpaper_photo g_photo;
static u8* g_photo_data = (u8*)0;
static int g_loaded_index = -1;   /* photo actuellement en memoire */
static int g_failed_index = -1;   /* dernier indice dont le chargement a echoue */

u32 wallpaper_photo_count()
{
    return WALLPAPER_COUNT;
}

const wallpaper_photo* wallpaper_photo_get(u32 index)
{
    if (index >= WALLPAPER_COUNT)
    {
        return (const wallpaper_photo*)0;
    }

    if (g_loaded_index == (int)index)
    {
        return &g_photo;
    }

    if (g_failed_index == (int)index)
    {
        return (const wallpaper_photo*)0;
    }

    asset_info info;

    if (!assets_find(WALLPAPERS[index].asset_name, &info)
        || info.width < 2 || info.height < 2
        || info.size != (u32)info.width * info.height * 3)
    {
        g_failed_index = (int)index;
        return (const wallpaper_photo*)0;
    }

    u32 size = 0;
    u8* data = assets_load(WALLPAPERS[index].asset_name, &size);

    if (data == (u8*)0)
    {
        g_failed_index = (int)index;
        return (const wallpaper_photo*)0;
    }

    /* Remplace la photo precedente seulement une fois la nouvelle lue. */

    if (g_photo_data != (u8*)0)
    {
        mk_free(g_photo_data);
    }

    g_photo_data = data;
    g_photo.name = WALLPAPERS[index].display_name;
    g_photo.rgb_data = data;
    g_photo.width = info.width;
    g_photo.height = info.height;
    g_loaded_index = (int)index;

    return &g_photo;
}
