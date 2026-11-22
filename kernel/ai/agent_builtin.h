#ifndef MIKEA_AGENT_BUILTIN_H
#define MIKEA_AGENT_BUILTIN_H

#include "agent.h"

/*
    ============================================================
    Mikea OS - Actions integrees de l'agent
    ============================================================

    Les 11 actions que l'agent peut faire, avec leur niveau de
    permission, la validation de leurs arguments et leurs messages.
    Elles n'appellent JAMAIS le systeme directement : tout passe par
    cette interface "plateforme", branchee sur le vrai systeme dans
    agent_kernel.c (et sur une fausse dans les tests hote).

        open_app       REVERSIBLE   ouvre une application
        set_theme      REVERSIBLE   clair / sombre / basculer       (annulable)
        set_sound      REVERSIBLE   son du systeme on / off          (annulable)
        set_wallpaper  REVERSIBLE   fond photo 1..N ou "suivant"     (annulable)
        system_info    READ         memoire / disque / heure
        list_files     READ         liste un dossier
        read_file      READ         debut d'un fichier
        create_file    WRITE        cree un fichier (+ contenu)      (annulable)
        restore_file   WRITE        sort un fichier de la corbeille  (annulable)
        delete_file    DESTRUCTIVE  met a la CORBEILLE (jamais efface) (annulable)
        logout         DESTRUCTIVE  ferme la session

    Aucune action ne peut supprimer definitivement quoi que ce soit :
    "delete_file" passe par la corbeille.
    ============================================================
*/

typedef struct
{
    int  (*open_app)(const char* canonical_name);                 /* 1 = ouverte */
    int  (*get_dark)(void);
    void (*set_dark)(int dark);
    int  (*get_sound)(void);
    void (*set_sound)(int on);
    u32  (*wallpaper_count)(void);
    u32  (*get_wallpaper)(void);
    void (*set_wallpaper)(u32 index);
    void (*sysinfo)(const char* what, char* out, u32 max);        /* "memoire" "disque" "heure" "tout" */
    u32  (*list_files)(const char* dir, char* out, u32 max);      /* ecrit les noms, renvoie le nombre */
    int  (*read_file)(const char* name, char* out, u32 max);      /* 1 = lu */
    int  (*file_exists)(const char* name);
    int  (*create_file)(const char* name, const char* content);   /* 1 = cree */
    int  (*trash_file)(const char* name);                         /* 1 = mis a la corbeille */
    int  (*restore_file)(const char* name);                       /* 1 = restaure */
    void (*logout)(void);
} agent_platform;

/* Branche la plateforme et installe le registre des actions integrees. */
void agent_builtin_install(const agent_platform* platform);

#endif
