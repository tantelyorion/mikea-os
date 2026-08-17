# MikeaOS

Un système d'exploitation x86_64 "from scratch" : bootloader maison,
noyau en C freestanding + assembleur, shell en ligne de commande,
système de fichiers, gestion des utilisateurs/permissions, un début
de gestionnaire de paquets et un format d'exécutable maison (MKX).

Pas de Linux, pas de GRUB, pas de libc : tout est écrit pour ce
projet, du tout premier octet exécuté par le BIOS jusqu'au shell.

---

## Sommaire

- [Ce que fait réellement MikeaOS aujourd'hui](#ce-que-fait-réellement-mikeaos-aujourdhui)
- [Ce qu'il ne fait PAS (encore)](#ce-quil-ne-fait-pas-encore)
- [Architecture](#architecture)
- [Structure du dépôt](#structure-du-dépôt)
- [Compiler le projet](#compiler-le-projet)
- [Lancer MikeaOS](#lancer-mikeaos)
- [Diagnostiquer un problème de démarrage](#diagnostiquer-un-problème-de-démarrage)
- [Utilisation : commandes disponibles](#utilisation--commandes-disponibles)
- [Dépannage courant](#dépannage-courant)
- [Pistes d'amélioration](#pistes-daméliration)

---

## Ce que fait réellement MikeaOS aujourd'hui

- **Démarrage** : secteur de boot BIOS maison (MBR) → chargeur
  "stage2" → passage réel 16 bits → protégé 32 bits → long mode
  64 bits → noyau C.
- **Noyau** : GDT/IDT/PIC configurés, gestion des interruptions,
  minuteur (PIT), pilote clavier PS/2, allocateur de tas simple.
- **Multitâche coopératif/préemptif hybride** : un ordonnanceur
  round-robin, préemption par le minuteur.
- **Système de fichiers** : pilote disque ATA (PIO) réel, structures
  bloc/inode/répertoire/superbloc — **mais pas encore persistant**
  d'un redémarrage à l'autre (voir plus bas).
- **Comptes utilisateurs et permissions** : connexion obligatoire,
  mots de passe hachés (FNV-1a, voir limites), permissions
  lecture/écriture/exécution par utilisateur.
- **Shell (`msh`)** : ligne de commande façon terminal Unix, avec un
  jeu de commandes de base (voir plus bas).
- **Gestionnaire de paquets (`mpm`)** : suivi de paquets installés en
  mémoire — pas encore d'installation binaire réelle.
- **MKX** : format d'exécutable minimal maison, chargé et exécuté par
  le noyau (`run <fichier>`), avec contrôle de permission `EXEC`.
- **Interface texte (`gui`)** : fenêtres dessinées en mode texte VGA
  (80×25 caractères), pas une interface graphique en pixels.

## Ce qu'il ne fait PAS (encore)

À prendre en compte avant toute démonstration ou usage sérieux :

| Domaine | État actuel |
|---|---|
| **Interface graphique** | Aucune (mode texte VGA uniquement, pas de pixels, pas de souris). `assets/` contient des icônes/polices/fonds d'écran réservés pour un futur pilote graphique, non utilisés. |
| **Applications par défaut** | Aucune (pas de calculatrice, pas d'explorateur de fichiers graphique, pas de panneau de réglages). Seul le shell `msh` fait office de terminal. |
| **Persistance disque** | Le pilote ATA réel existe, mais la table des inodes/répertoires reste en RAM : **tous les fichiers créés sont perdus au redémarrage.** |
| **Isolation mémoire / Ring 3** | Aucune. Tout (shell, commandes, programmes `.mkx`) s'exécute au niveau privilège noyau. |
| **Sécurité des mots de passe** | Hash FNV-1a (rapide, non cryptographique) — pas de SHA-256/bcrypt. |
| **Gestionnaire de paquets** | `mpm install` ne fait qu'enregistrer un nom/version en mémoire, aucune extraction/installation binaire réelle. |
| **Détection matérielle** | `cpu_init()`/`pci_init()` sont des stubs (pas de CPUID, pas de scan PCI) ; pilotes à ports I/O fixes. |
| **SDK applicatif** | `apps/hello/hello.c` existe en exemple mais n'est pas compilé par le Makefile. |

## Architecture

```
BIOS
 └─ boot/bios/boot.asm       (secteur de boot, 512 octets, LBA 0)
     └─ boot/loader/stage2.asm   (LBA 1-64, 32 Ko fixes)
         │  mode réel 16 bits → protégé 32 bits → long mode 64 bits
         │  charge kernel.bin depuis le disque (LBA 97+) et saute à 0x100000
         └─ kernel/arch/x86_64/entry.asm  (_start, point d'entrée à 0x100000)
             └─ kernel/kernel.c : kernel_start()
                 ├─ console, GDT, IDT, IRQ, minuteur, clavier, PCI
                 ├─ processus/threads/ordonnanceur
                 ├─ filesystem (mkfs_init)
                 ├─ mkx (chargeur d'exécutables)
                 ├─ mpm (paquets)
                 ├─ utilisateurs/mots de passe/permissions
                 └─ shell/msh.c : msh_start()
                     └─ security/login.c : écran de connexion obligatoire
                         └─ boucle de commandes (shell/commands.c)
```

L'image disque de démarrage (`build/MikeaOS.img`) est un **disque brut**
(secteur de boot + stage2 + noyau collés bout à bout), **pas** une vraie
image ISO9660/El Torito malgré une ancienne convention de nommage —
d'où l'extension `.img` désormais utilisée. Un second fichier,
`build/disk.img`, sert de disque de données séparé pour le système de
fichiers.

## Structure du dépôt

```
boot/            Bootloader (BIOS MBR + stage2) et installeur (pas encore fait)
kernel/          Noyau : arch (entrée bas niveau), console, cpu, drivers,
                 input, interrupt, memory (tas), process (threads/ordonnanceur)
filesystem/      Pilote disque ATA, blocs, inodes, répertoires, fichiers, mkfs
security/        Connexion, mots de passe, permissions, comptes utilisateurs
shell/           Boucle de commandes (msh) et implémentation des commandes
packages/        Gestionnaire de paquets (mpm), base de données, installeur
mkx/             Format d'exécutable maison : format, chargeur, runtime
libc/            Fonctions chaîne minimales (memcpy, strcmp...)
gui/             Fenêtres en mode texte VGA
sdk/             En-tête pour applications tierces (mikea_sdk.h)
apps/            Exemple d'application (hello) — pas encore compilé
assets/          Icônes/polices/fond d'écran réservés pour un futur pilote graphique
include/         Types de base partagés (u8/u16/u32/u64...)
demo-host/       Bac à sable : compile certains modules (gui, console...) avec
                 un compilateur hôte normal pour les tester hors noyau
tools/           tools/check_toolchain.sh : vérifie les outils installés
scripts/         build.sh / run.sh : raccourcis de compilation/lancement
```

## Compiler le projet

### Outils nécessaires

- `nasm` (assembleur)
- Un compilateur C capable de produire de l'ELF64 x86-64 en freestanding :
  - **`gcc` normal** si vous compilez sur un hôte x86_64 (Linux, WSL2,
    macOS Intel) — pas besoin de compilateur croisé dédié.
  - **`clang` + `lld` (LLVM)** sur n'importe quel hôte, y compris
    Windows : Clang cible n'importe quelle architecture/format nativement
    via `--target=`, sans compilateur croisé à construire ou télécharger.
  - `x86_64-elf-gcc`/`x86_64-elf-ld`/`x86_64-elf-objcopy` si vous en
    disposez déjà (compilateur croisé dédié, ex. via Homebrew sur macOS
    Apple Silicon).
- `make`
- `qemu-system-x86_64` (pour tester)
- `dd`/`truncate` (déjà présents sur Linux/macOS/MSYS2)

`bash tools/check_toolchain.sh` liste ce qui est installé et ce qui manque.

### Sur Linux / WSL2 / macOS (hôte x86_64)

```bash
make all
```
`CC`/`LD`/`OBJCOPY` valent respectivement `gcc`/`ld`/`objcopy` par défaut :
rien à installer en plus de `nasm`, `make` et un compilateur déjà présent
sur le système.

### Sur Windows, avec LLVM/Clang (MSYS2, Git Bash...)

Le `gcc`/`clang` par défaut sur Windows produit du **PE** (format Windows),
pas de l'**ELF** (format attendu par ce noyau). Il faut donc forcer la
cible avec Clang. **Important** : `ld.lld` appelé directement échoue
souvent sur Windows (`unknown argument: -T`) — il faut faire piloter
l'édition de liens par `clang` lui-même via `-fuse-ld=lld` :

```bash
make CC=clang LD="clang --target=x86_64-elf -fuse-ld=lld -nostdlib" LD_FLAGS="-Wl,-T,linker.ld -Wl,-z,max-page-size=0x1000" OBJCOPY=llvm-objcopy TARGET_FLAG=--target=x86_64-elf all
```

### Avec un compilateur croisé dédié (`x86_64-elf-gcc`)

```bash
make CC=x86_64-elf-gcc LD=x86_64-elf-ld OBJCOPY=x86_64-elf-objcopy all
```

### Résultat

`make all` produit :
- `build/MikeaOS.img` — l'image de démarrage (boot + stage2 + noyau), 1 Mo.
- `build/obj/` — les fichiers objets intermédiaires.

`make clean` supprime tout `build/`.

## Lancer MikeaOS

### QEMU (recommandé, cible principale du projet)

```bash
make run
# ou, si vous avez utilisé CC=clang plus haut, repassez les mêmes variables :
make CC=clang LD="clang --target=x86_64-elf -fuse-ld=lld -nostdlib" LD_FLAGS="-Wl,-T,linker.ld -Wl,-z,max-page-size=0x1000" OBJCOPY=llvm-objcopy TARGET_FLAG=--target=x86_64-elf run
# ou directement :
bash scripts/run.sh
```

Cela crée aussi `build/disk.img` (1 Mo, disque de données vide) si besoin,
et lance :
```bash
qemu-system-x86_64 \
  -drive format=raw,file=build/MikeaOS.img \
  -drive format=raw,file=build/disk.img
```
**Toujours `-drive format=raw`, jamais `-cdrom`** — ce n'est pas une vraie
image optique.

Pour quitter : fermez la fenêtre, ou `Ctrl+Alt+2` (console moniteur QEMU)
puis `quit`.

### VirtualBox

`build/MikeaOS.img` doit être attaché comme **disque dur**, jamais monté
comme DVD (il n'y a aucun catalogue de démarrage El Torito dedans — le
monter comme DVD provoque l'erreur *"no bootable medium found"*).

```bash
VBoxManage convertfromraw build/MikeaOS.img build/MikeaOS.vdi --format VDI
```
Puis dans VirtualBox : Contrôleur de stockage → **Disque dur** (pas
lecteur optique) → attacher `MikeaOS.vdi`. Faites de même pour
`build/disk.img` sur un second contrôleur si vous voulez la persistance
(actuellement non fonctionnelle de toute façon, voir plus haut).

### Compte par défaut

```
login: root
mot de passe: mikea
```

> **État confirmé** : démarrage complet + connexion `root`/`mikea`
> testés avec succès sous QEMU (Windows/MSYS2 MINGW64, Clang/lld).

## Diagnostiquer un problème de démarrage

Le démarrage affiche des messages de contrôle à chaque étape critique,
pour localiser immédiatement où ça bloque :

```
Mikea OS Bootloader
Loading Kernel...
[1/2] Extensions BIOS OK              <- boot.asm : extensions LBA détectées
[2/2] Stage2 charge, saut...          <- boot.asm : stage2.bin chargé, saut effectué
[stage2] demarre                      <- stage2.asm atteint et exécuté
[stage2] noyau charge, passage 32 bits...   <- kernel.bin chargé depuis le disque
[stage2] VBE detecte (mode graphique disponible)   <- ou : VBE non disponible (mode texte conservé)
[stage2] Mode protege OK              <- transition 32 bits réussie
[stage2] Pagination OK, passage 64 bits...  <- PAE/pagination/long mode configurés
[stage2] Appel du noyau...            <- saut vers _start (kernel/arch/x86_64/entry.asm)
```

Le dernier message affiché indique où chercher :

- **Rien du tout, écran noir** : le secteur de boot n'a même pas été
  exécuté — vérifiez que `build/MikeaOS.img` est bien attaché en
  `format=raw` (QEMU) ou comme disque dur (VirtualBox), pas monté comme
  média optique.
- **S'arrête avant `[1/2]`** : le BIOS ne supporte pas les extensions
  LBA (très rare aujourd'hui) — matériel/émulateur non standard.
- **S'arrête entre `[2/2]` et `[stage2] demarre`** : le saut vers stage2
  ou ses tout premiers octets posent problème — le fichier `.img` a
  peut-être été tronqué/corrompu lors d'une copie.
- **`[stage2] Erreur de lecture disque (noyau)`** : la lecture LBA du
  noyau échoue. Deux causes possibles, déjà corrigées dans ce dépôt :
  fichier `.img` trop court (voir `truncate -s 1M` dans le Makefile),
  ou plus de 127 secteurs demandés en un seul appel `INT13h AH=0x42`
  (la plupart des BIOS, dont SeaBIOS, refusent — voir le découpage en
  6 appels de 100 secteurs dans `stage2.asm`).
- **S'arrête entre `[stage2] noyau charge...` et `VBE detecte`/`VBE non
  disponible`** : la détection VESA/VBE (nouvelle, voir « Interface
  graphique » plus bas) pose problème — signalez-le, c'est du code
  récent non testé sur matériel réel.
- **`VBE non disponible` s'affiche alors que vous attendiez des
  graphismes** : normal pour l'instant — cette étape ne fait que
  détecter, elle ne bascule jamais réellement l'affichage (voir la
  section Interface graphique). Le mode texte continue de fonctionner
  dans les deux cas.
- **S'arrête entre `[stage2] noyau charge...` et `Mode protege OK`** :
  problème dans le GDT ou l'activation du mode protégé (`mov cr0`).
- **S'arrête entre `Mode protege OK` et `Pagination OK`** : la copie du
  noyau vers 1 Mo ou la configuration PAE/pagination/MSR EFER a un
  problème — le symptôme typique est un redémarrage immédiat de la VM
  (retour à "Booting from hard disk...", signe d'un triple fault).
- **`Appel du noyau...` s'affiche mais rien ensuite** (pas d'écran de
  connexion `root`/`mikea`) : le bas niveau fonctionne, le problème est
  dans le noyau C lui-même (`kernel/kernel.c`, `kernel_start()`).

## Utilisation : commandes disponibles

```
help        about       version     clear       cpu         mem
whoami      useradd     userdel     passwd      users       logout
ps          mpm         mkdir       touch       write       cat
rm          ls          gui         run
```

Tapez `help` dans le shell pour la liste à jour avec une courte
description de chaque commande.

## Dépannage courant

| Symptôme | Cause probable |
|---|---|
| `x86_64-elf-gcc: No such file or directory` | Compilateur croisé absent — utilisez `CC=gcc` (hôte x86_64) ou `CC=clang ... TARGET_FLAG=--target=x86_64-elf`. |
| `lld: error: unknown argument: -T` (Windows) | `ld.lld` appelé directement ne se comporte pas en éditeur de liens GNU/ELF sur Windows — pilotez le lien via `clang -fuse-ld=lld` (voir la commande de compilation Windows plus haut : `LD="clang --target=x86_64-elf -fuse-ld=lld -nostdlib"` et `LD_FLAGS="-Wl,-T,linker.ld -Wl,-z,max-page-size=0x1000"`). |
| VirtualBox : *"no bootable medium found"* | `MikeaOS.img` monté comme DVD au lieu de disque dur — voir [VirtualBox](#virtualbox) plus haut. |
| QEMU bloqué sur `Loading kernel....` sans aucun message `[1/2]` | Le fichier `.img` n'est probablement pas attaché en `format=raw`, ou provient d'une build incomplète — relancez `make clean && make all`. |
| `[stage2] Erreur de lecture disque (noyau)` malgré un fichier `.img` de 1 Mo | Assurez-vous d'utiliser la version du dépôt avec le découpage en 6×100 secteurs (voir ce README, section diagnostic) — c'était le second bug, indépendant de la taille du fichier. |
| Le clavier semble se figer après quelques secondes d'utilisation | Bug corrigé (course rare sur le drapeau IF, voir `kernel/process/thread.c`) — vérifiez que vous utilisez bien la version corrigée. |
| `root`/`mikea` refusés à l'écran de connexion (« Login incorrect » en boucle) | Bug corrigé : le tampon clavier n'était jamais vidé avant l'écran de connexion — une touche pressée pendant le démarrage (ou lors d'un essai précédent raté) polluait la saisie suivante. Voir `kernel/drivers/keyboard/keyboard.c` (`keyboard_flush()`). Évitez aussi de taper avant que `Mikea OS login:` ne soit affiché. |
| Redémarrage en boucle juste après l'activation du mode graphique (« Booting from hard disk... » qui revient sans arrêt) | Bug corrigé : les tables de pages ne couvraient que les 64 premiers Mo de RAM, alors que le framebuffer VBE est presque toujours placé bien plus haut en mémoire physique (~3,5-4 Go) — toute écriture dessus déclenchait un triple fault. Les tables de pages couvrent désormais tout l'espace physique 32 bits (4 Go). Si ça persiste malgré une recompilation complète, désactivez temporairement l'activation réelle du mode dans `stage2.asm` (repassez au comportement « détection seule » de l'étape 1) et signalez-le. |

## Interface graphique (en cours)

Un chantier est en cours pour passer du mode texte VGA à une véritable
interface graphique en pixels (VESA/VBE), dans le style sobre d'un
écran d'ordinateur de vaisseau spatial (pas de "neon" — futuriste et
minimaliste). Avancement :

- ✅ **Étape 1 — Détection VBE** (`boot/loader/stage2.asm`) : vérifie
  que le BIOS supporte VESA/VBE 2.0+, cherche un mode graphique linéaire
  en couleur directe (24 bits ou plus, résolution entre 640 et 1280 de
  large) et enregistre ses caractéristiques (adresse physique du
  framebuffer, largeur/hauteur/profondeur, position des canaux
  rouge/vert/bleu) dans un tampon fixe (adresse physique `0x7E00`) pour
  que le noyau C puisse les lire. **Cette étape ne change PAS le mode
  vidéo réel** — l'écran continue de fonctionner en mode texte BIOS
  classique dans tous les cas, pour ne prendre aucun risque sur ce qui
  fonctionne déjà (démarrage, connexion, shell).
- ✅ **Pilote graphique C** (`kernel/drivers/graphics/`) : lit ces
  informations, expose `gfx_put_pixel`/`gfx_fill_rect`/`gfx_draw_rect`/
  `gfx_draw_text` etc. Police bitmap 8×8 intégrée
  (`assets/fonts/font8x8_basic.h`, domaine public,
  [dhepper/font8x8](https://github.com/dhepper/font8x8)). `graphics_init()`
  affiche seulement un message d'état au démarrage (`[graphics] ...`) —
  **aucun pixel ne s'affiche encore**, tant que le mode vidéo réel n'est
  pas basculé (voir étape suivante).
- ✅ **Étape 2 — Bascule réelle vers le mode graphique** : `stage2.asm`
  active désormais réellement le mode VBE trouvé (`INT10h AX=0x4F02`)
  juste avant le passage en mode protégé. `kernel/console/console.c`
  bascule automatiquement tout l'affichage texte (`console_write`,
  `console_backspace`, `console_clear`) vers le rendu pixel dès que
  `gfx_available()` est vrai — **aucun appelant existant** (connexion,
  shell, commandes) n'a eu besoin d'être modifié. **Sécurité** : si
  l'activation du mode échoue, le système bascule automatiquement sur
  le mode texte classique, exactement comme si VBE n'existait pas —
  aucun risque d'écran noir bloquant.
- ⏳ **Étape 3 — Habillage visuel** (polices, couleurs, disposition).
- ⏳ **Étape 4 — Assistant graphique d'installation.**
- ⏳ **Étape 3 — Pilote souris** ✅ : `kernel/drivers/mouse/` (protocole
  PS/2 standard, IRQ12, paquets 3 octets). Position bornée à l'écran
  quand le mode graphique est actif. Commande `mouse` pour tester
  (affiche 10 lectures successives — déplacez la souris pendant que
  ça tourne).
- ⏳ **Étape 4 — Abstraction fenêtre + rendu empilé.**
- ⏳ **Étape 5 — Interaction souris ↔ fenêtres** ✅ : `gui` affiche
  désormais un curseur souris en temps réel et un bouton de fermeture
  ('X') cliquable dans la barre de titre, en plus de la fermeture au
  clavier (Entrée). Boucle de rendu cadencée (~30 ips) pour éviter de
  saturer le CPU.
- ⏳ **Étape 6 — Éléments d'interface + style visuel final.**

Pourquoi cette prudence : une fois le mode vidéo réellement changé, le
tampon texte `0xB8000` (utilisé par tout l'affichage actuel : messages
de démarrage, écran de connexion, shell) cesse instantanément de
s'afficher, puisque la carte vidéo n'est plus en mode texte. Basculer
avant d'avoir un remplaçant graphique fonctionnel donnerait un écran
noir sans aucun moyen de se connecter.

## Pistes d'amélioration

Par ordre de priorité suggéré pour transformer ce projet en OS plus
abouti (aucun n'est un correctif rapide — ce sont des chantiers à part
entière) :

1. **Persistance réelle du système de fichiers** — le plus proche
   d'être terminé : `superblock_write()`/`superblock_load()` existent
   déjà, il reste à sérialiser la table des inodes/répertoires.
2. **Isolation mémoire (Ring 3, appels système)** — refonte
   architecturale majeure, prérequis pour une exécution sûre de code
   utilisateur.
3. **Hachage de mot de passe cryptographique** (SHA-256/bcrypt) à la
   place de FNV-1a.
4. **Gestionnaire de paquets fonctionnel** — format `.mpk` réel,
   extraction, vérification de signature.
5. **Détection matérielle dynamique** (CPUID, scan PCI) à la place des
   pilotes à ports fixes.
6. **Un vrai pilote graphique** (VESA/VBE ou GOP) — les assets
   (`assets/icons`, `assets/wallpapers`, `assets/fonts`) sont déjà en
   place et n'attendent que ça.
