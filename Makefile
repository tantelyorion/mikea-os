# ============================================================
#
#                 Mikea OS Build System
#
#                 Architecture : x86_64
#
#                 Developer :
#                 Tantely Orion
#
# ============================================================


# ------------------------------------------------------------
# Tools
# ------------------------------------------------------------


ASM = nasm


# Correctif (installation plus simple) : ce noyau cible x86_64,
# la MEME architecture que la quasi-totalite des machines de
# developpement actuelles (PC, WSL2, la plupart des VM). Sur un
# hote x86_64, le gcc/ld/objcopy standard du systeme produit deja
# du code et des objets ELF64 x86-64 -- exactement ce dont ce
# projet a besoin -- sans qu'un compilateur croise "x86_64-elf-*"
# (long et complexe a construire soi-meme, voir
# tools/check_toolchain.sh) soit necessaire. -ffreestanding et
# -fno-pie (deja dans C_FLAGS/LD_FLAGS ci-dessous) empechent de
# toute facon toute dependance a la libc ou au chargeur dynamique
# de l'hote.
#
# CC/LD/OBJCOPY restent personnalisables (ex. "make CC=x86_64-elf-gcc
# LD=x86_64-elf-ld OBJCOPY=x86_64-elf-objcopy") pour qui compile
# depuis un hote d'une autre architecture (ex. Apple Silicon/ARM) et
# dispose donc d'un vrai compilateur croise.
#
# Correctif (echec "cc: No such file or directory" sous MinGW/Git
# Bash sur Windows, ou tout hote sans binaire "cc") : "CC ?= gcc"
# NE PROTEGE PAS contre ce cas. GNU Make predefinit CC="cc" via ses
# regles implicites AVANT meme de lire ce Makefile, avec
# origin(CC) = "default" -- et "?=" ne fait "prendre la valeur que
# si la variable n'est pas deja definie", ce qui est deja le cas
# ici (elle vaut "cc", juste pas via une affectation explicite de ce
# fichier). Le Makefile utilisait donc silencieusement le "cc" de
# Make, jamais le "gcc" ecrit ci-dessous -- inoffensif sur les
# systemes ou "cc" est un alias de "gcc" (la plupart des distros
# Linux), mais en erreur des qu'aucun binaire "cc" n'existe (MinGW/
# Git Bash sous Windows : seul "gcc.exe" y est present). On verifie
# explicitement l'origine de la variable : si elle vient encore des
# regles par defaut de Make (personne n'a rien precise), on la fixe
# nous-memes a "gcc" ; si l'utilisateur l'a fournie sur la ligne de
# commande ou via l'environnement (ex. CC=x86_64-elf-gcc), on la
# laisse intacte. Meme raisonnement pour LD (Make predefinit
# egalement LD="ld", donc "LD ?= ld" avait la meme faille -- sans
# consequence ici puisque la valeur voulue est la meme, mais corrige
# par souci de coherence/robustesse). OBJCOPY n'est pas une variable
# implicite de Make : "OBJCOPY ?= objcopy" n'a jamais eu ce probleme.

# Detection automatique : si "gcc" est absent (cas typique de Git Bash /
# MINGW64 sous Windows) mais que clang + lld sont installes, on bascule
# sur la chaine LLVM, qui sait produire de l'ELF pour le noyau.
HAVE_GCC   := $(shell command -v gcc 2>/dev/null)
HAVE_CLANG := $(shell command -v clang 2>/dev/null)
USE_LLVM   :=
ifeq ($(origin CC),default)
ifeq ($(HAVE_GCC),)
ifneq ($(HAVE_CLANG),)
USE_LLVM := 1
endif
endif
endif

ifeq ($(USE_LLVM),1)
CC := clang
# On appelle le linker ELF de LLVM DIRECTEMENT (ld.lld). Passer par
# "clang -fuse-ld=lld" sous MINGW64 echoue : clang relance gcc/collect2,
# qui lance lld en mode COFF ("unknown argument: -T").
# Sous MSYS2/MINGW64, "ld.lld" demarre lld en mode COFF ("unknown argument:
# -T") : on force donc explicitement le mode ELF avec "lld -flavor gnu".
ifneq ($(shell command -v lld 2>/dev/null),)
LD := lld -flavor gnu
else
LD := ld.lld
endif
OBJCOPY ?= llvm-objcopy
else
ifeq ($(origin CC),default)
CC := gcc
endif

ifeq ($(origin LD),default)
LD := ld
endif

OBJCOPY ?= objcopy
endif


# Correctif (compilation croisee sans outil dedie a installer) :
# contrairement a GCC, Clang est un compilateur croise "nativement"
# -- un seul binaire "clang" sait produire du code pour n'importe
# quelle architecture cible via --target, sans avoir besoin d'etre
# reconstruit specifiquement (voir la doc LLVM : "LLVM Cross-
# Compiler"). Combine a "lld" (l'editeur de liens LLVM, qui
# detecte automatiquement le format ELF64 x86-64 depuis les
# fichiers objets fournis) et "llvm-objcopy" (memes options que le
# objcopy GNU, dont "-O binary"), cela permet de compiler ce noyau
# avec une installation LLVM/Clang standard, meme sur un hote qui
# n'est PAS x86_64, sans jamais avoir a construire ou telecharger
# un compilateur croise "x86_64-elf-gcc" dedie.
#
# Correctif (echec de link/format sur Windows avec Clang) : la
# phrase precedente ("TARGET_FLAG utile seulement sur un hote d'une
# autre architecture") etait incomplete. Le probleme n'est PAS
# l'architecture CPU (un PC Windows est deja x86_64) mais le FORMAT
# D'OBJET par defaut de l'hote : l'installateur LLVM officiel pour
# Windows fait cibler a "clang", par defaut, le triplet natif
# Windows (x86_64-pc-windows-msvc) -- qui produit des objets COFF
# (.obj), incompatibles avec les objets ELF64 produits par nasm
# (voir ASM_FLAGS, "-f elf64" plus bas) et attendus par ld.lld/
# llvm-objcopy. Meme chose sur macOS (format Mach-O par defaut).
# Sans --target=x86_64-elf explicite, la compilation C reussit
# silencieusement mais l'edition de liens echoue (formats d'objets
# incompatibles entre l'assembleur et le compilateur). On detecte
# donc ici Windows ($OS, variable d'environnement standard) et
# macOS (uname -s = Darwin) et on force alors --target=x86_64-elf
# par defaut des que CC contient "clang" -- un TARGET_FLAG fourni
# explicitement sur la ligne de commande reste toujours prioritaire.

ifneq (,$(findstring clang,$(CC)))
ifeq ($(TARGET_FLAG),)
ifeq ($(OS),Windows_NT)
TARGET_FLAG := --target=x86_64-elf
else ifeq ($(shell uname -s 2>/dev/null),Darwin)
TARGET_FLAG := --target=x86_64-elf
endif
endif
endif

TARGET_FLAG ?=


# Choix du backend audio QEMU (carte son "-device sb16", voir
# kernel/drivers/soundblaster) : QEMU ne devine pas tout seul quel
# systeme audio de l'hote utiliser, et le mauvais choix ne fait pas
# echouer QEMU mais le fait tourner MUET, silencieusement -- un
# echec bien plus deroutant a diagnostiquer qu'une erreur franche.
# Meme raisonnement que la detection Windows/macOS du bloc Clang
# ci-dessus : "pulseaudio" convient a la grande majorite des
# distributions Linux de bureau actuelles (y compris via le module
# de compatibilite PipeWire-Pulse, tres largement deploye par
# defaut), "coreaudio" est le backend natif macOS, et "wasapi"
# le backend audio moderne de Windows.
#
# Correctif (son de demarrage muet sous Windows) : "dsound"
# (DirectSound) etait utilise par defaut ici, mais s'est revele
# silencieux/instable sur QEMU recent (>= 6.x) sur plusieurs
# machines Windows testees, sans qu'aucune erreur ne soit
# remontee -- le pire cas de figure decrit juste au-dessus.
# "wasapi" (Windows Audio Session API), disponible depuis QEMU
# 5.0, est le backend audio natif moderne de Windows et s'est
# montre nettement plus fiable en pratique. QEMU_AUDIODEV reste
# personnalisable (ex. "make run QEMU_AUDIODEV=dsound" pour
# revenir a l'ancien comportement, "make run
# QEMU_AUDIODEV=alsa" sur un Linux sans PulseAudio/PipeWire, ou
# "sdl"/"none" pour un test sans materiel audio du tout).

ifeq ($(origin QEMU_AUDIODEV),undefined)
ifeq ($(OS),Windows_NT)
# Correctif : beaucoup de builds QEMU pour Windows n'incluent PAS "wasapi"
# (erreur : "Parameter 'driver' does not accept value 'wasapi'"). On
# demande a QEMU ce qu'il sait faire : dsound, wasapi, sdl, sinon "none".
QEMU_AUDIO_LIST_WIN := $(shell qemu-system-x86_64 -audiodev help 2>/dev/null)
ifneq (,$(filter dsound,$(QEMU_AUDIO_LIST_WIN)))
QEMU_AUDIODEV := dsound
else ifneq (,$(filter wasapi,$(QEMU_AUDIO_LIST_WIN)))
QEMU_AUDIODEV := wasapi
else ifneq (,$(filter sdl,$(QEMU_AUDIO_LIST_WIN)))
QEMU_AUDIODEV := sdl
else
QEMU_AUDIODEV := none
endif
else ifeq ($(shell uname -s 2>/dev/null),Darwin)
QEMU_AUDIODEV := coreaudio
else
# Correctif (QEMU refuse de demarrer sur un Linux sans PulseAudio) :
# "-audiodev pa" fait ECHOUER QEMU (et non pas tourner muet) si le
# backend n'est pas disponible. On interroge donc QEMU lui-meme sur les
# backends audio compiles, et on retombe sur "none" si aucun ne convient.
QEMU_AUDIO_LIST := $(shell qemu-system-x86_64 -audiodev help 2>/dev/null)
ifneq (,$(filter pa,$(QEMU_AUDIO_LIST)))
QEMU_AUDIODEV := pa
else ifneq (,$(filter alsa,$(QEMU_AUDIO_LIST)))
QEMU_AUDIODEV := alsa
else ifneq (,$(filter sdl,$(QEMU_AUDIO_LIST)))
QEMU_AUDIODEV := sdl
else
QEMU_AUDIODEV := none
endif
endif
endif



# ------------------------------------------------------------
# Flags
# ------------------------------------------------------------


ASM_FLAGS = -f elf64


C_FLAGS = \
$(TARGET_FLAG) \
-ffreestanding \
-mno-red-zone \
-mno-mmx \
-mno-sse \
-mno-sse2 \
-fno-stack-protector \
-fno-pie \
-Wall \
-Wextra \
-I. \
-Iinclude \
-MMD -MP \
-c


LD_FLAGS = \
-T linker.ld \
-z max-page-size=0x1000



# ------------------------------------------------------------
# Directories
# ------------------------------------------------------------


BUILD = build

OBJDIR = $(BUILD)/obj

# Correctif : cree toujours les dossiers de build, meme pour 'make run'
# (sinon nasm echoue : "unable to open output file build/boot.bin").
$(shell mkdir -p $(BUILD) $(OBJDIR))

# Carte reseau emulee par QEMU : e1000 (defaut) ou rtl8139  ->  make run QEMU_NIC=rtl8139
QEMU_NIC ?= e1000


BOOT = boot/bios

KERNEL = kernel



# Correctif (piege VirtualBox) : ce fichier n'est PAS une
# vraie image ISO9660/El Torito (pas de catalogue de
# demarrage, pas de systeme de fichiers CD) -- c'est un
# disque brut (secteur de boot + stage2 + noyau colles bout
# a bout), fait pour etre branche comme un disque DUR (voir
# la cible "run" plus bas : "-drive format=raw", jamais
# "-cdrom"). L'ancien nom "MikeaOS.iso" laissait croire le
# contraire : monter ce fichier dans un lecteur optique
# virtuel (VirtualBox, reflexe naturel vu l'extension .iso)
# echoue immediatement avec "no bootable medium found",
# puisqu'il n'y a aucun catalogue El Torito a y trouver.
# ".img" reflete la nature reelle du fichier.

IMG = $(BUILD)/MikeaOS.img




# ------------------------------------------------------------
# Files
# ------------------------------------------------------------


BOOT_BIN = $(BUILD)/boot.bin

STAGE2_BIN = $(BUILD)/stage2.bin

ENTRY_OBJ = $(OBJDIR)/entry.o

CTX_SWITCH_OBJ = $(OBJDIR)/context_switch.o

ISR_STUBS_OBJ = $(OBJDIR)/isr_stubs.o


# Toutes les sources C du projet : kernel, filesystem, shell,
# securite, gestionnaire de paquets, format mkx, applications.
#
# Avant ce correctif, seul kernel/kernel.c etait compile ici :
# le filesystem, le shell, la securite, les processus/threads,
# mkx et les paquets n'etaient JAMAIS assembles dans le binaire
# final, meme si tout le code source existait.

# libc et gui sont desormais compiles avec le noyau : avant,
# ces deux dossiers etaient vides et n'existaient pas encore.
#
# apps/ n'est PAS compile ici : ce sont des programmes
# utilisateur au format MKX, distincts du noyau (a
# construire separement avec le futur outil mkx). sdk/ est
# uniquement compose de headers (pas de .c a compiler).
C_SOURCES = $(shell find kernel filesystem shell security packages mkx libc gui net crypto boot/installer apps -name "*.c" 2>/dev/null)

C_OBJECTS = $(patsubst %.c,$(OBJDIR)/%.o,$(C_SOURCES))

ALL_OBJECTS = $(ENTRY_OBJ) $(CTX_SWITCH_OBJ) $(ISR_STUBS_OBJ) $(C_OBJECTS)


KERNEL_ELF = $(BUILD)/kernel.elf

KERNEL_BIN = $(BUILD)/kernel.bin




# ------------------------------------------------------------
# Default target
# ------------------------------------------------------------


all: clean dirs $(IMG)



# ------------------------------------------------------------
# Create directories
# ------------------------------------------------------------


dirs:

	mkdir -p $(BUILD)

	mkdir -p $(OBJDIR)



# ------------------------------------------------------------
# Boot sector
# ------------------------------------------------------------


$(BOOT_BIN): $(BOOT)/boot.asm

	$(ASM) \
	$(BOOT)/boot.asm \
	-f bin \
	-o $(BOOT_BIN)



# ------------------------------------------------------------
# Stage 2 Loader
# ------------------------------------------------------------


$(STAGE2_BIN): boot/loader/stage2.asm

	$(ASM) \
	boot/loader/stage2.asm \
	-f bin \
	-o $(STAGE2_BIN)



# ------------------------------------------------------------
# Kernel Entry
# ------------------------------------------------------------


$(ENTRY_OBJ): $(KERNEL)/arch/x86_64/entry.asm

	mkdir -p $(dir $@)

	$(ASM) \
	$(KERNEL)/arch/x86_64/entry.asm \
	$(ASM_FLAGS) \
	-o $(ENTRY_OBJ)



# ------------------------------------------------------------
# Context switch (changement de contexte - multitache)
# ------------------------------------------------------------


$(CTX_SWITCH_OBJ): $(KERNEL)/process/context_switch.asm

	mkdir -p $(dir $@)

	$(ASM) \
	$(KERNEL)/process/context_switch.asm \
	$(ASM_FLAGS) \
	-o $(CTX_SWITCH_OBJ)



# ------------------------------------------------------------
# ISR / IRQ stubs (points d'entree des interruptions)
# ------------------------------------------------------------


$(ISR_STUBS_OBJ): $(KERNEL)/cpu/isr_stubs.asm

	mkdir -p $(dir $@)

	$(ASM) \
	$(KERNEL)/cpu/isr_stubs.asm \
	$(ASM_FLAGS) \
	-o $(ISR_STUBS_OBJ)



# ------------------------------------------------------------
# Compilation generique de chaque fichier .c du projet
# ------------------------------------------------------------


$(OBJDIR)/%.o: %.c

	mkdir -p $(dir $@)

	$(CC) \
	$(C_FLAGS) \
	$< \
	-o $@



# ------------------------------------------------------------
# Link Kernel
# ------------------------------------------------------------


$(KERNEL_ELF): $(ALL_OBJECTS)

	$(LD) \
	$(LD_FLAGS) \
	$(ALL_OBJECTS) \
	-o $(KERNEL_ELF)



# ------------------------------------------------------------
# Convert ELF -> Binary
# ------------------------------------------------------------


$(KERNEL_BIN): $(KERNEL_ELF)

	$(OBJCOPY) \
	-O binary \
	$(KERNEL_ELF) \
	$(KERNEL_BIN)

	@size=$$(wc -c < $(KERNEL_BIN)); \
	if [ $$size -gt 512000 ]; then \
	echo "ERREUR : kernel.bin ($$size octets) depasse les 1000 secteurs lus par stage2 (512000 octets)."; exit 1; fi



# ------------------------------------------------------------
# Create disk image (image disque brute, pas une vraie ISO9660)
# ------------------------------------------------------------


$(IMG): $(BOOT_BIN) $(STAGE2_BIN) $(KERNEL_BIN) $(wildcard assets/assets.pak)

	cat \
	$(BOOT_BIN) \
	$(STAGE2_BIN) \
	$(KERNEL_BIN) \
	> $(IMG)

	# Correctif critique (lecture disque du noyau echoue) :
	# boot.asm+stage2.asm+kernel.bin ne remplit ce fichier que
	# jusqu'a sa taille exacte (en general a peine plus que
	# 49664 + quelques dizaines de Ko), alors que dap_kernel
	# (boot/loader/stage2.asm) demande TOUJOURS 900 secteurs
	# (450 Ko) a partir du LBA 97, quelle que soit la taille
	# reelle du noyau (marge volontairement large, voir le
	# commentaire de dap_kernel). Sans ce correctif, ce fichier
	# etait donc systematiquement plus court que ce que la
	# lecture demande : QEMU/le BIOS refusent de lire au-dela
	# de la fin reelle du fichier, INT13h AH=0x42 renvoie une
	# erreur (drapeau carry), et stage2 affiche "Erreur de
	# lecture disque (noyau)" avant de figer la machine -- le
	# noyau n'etait jamais charge, meme quand la compilation
	# reussissait parfaitement. On complete donc ce fichier a
	# 1 Mo (tres largement au-dessus des 510464 octets requis,
	# de quoi laisser une bonne marge de croissance au noyau).

	truncate -s 1M $(IMG)

	# Pack d'assets (fonds d'ecran, sons) : place juste apres la zone du
	# noyau (secteurs 97 a 1096), au secteur 1100 -- voir
	# kernel/assets/assets.h (ASSETS_PAK_LBA) et tools/pack_assets.py.
	# Reste sous INSTALLER_SECTOR_COUNT (2048) : la commande "install" le
	# copie donc avec le reste de l'image. S'il est absent, le systeme
	# demarre quand meme (fonds en degrade, bips au lieu des sons).
	@if [ -f assets/assets.pak ]; then \
		dd if=assets/assets.pak of=$(IMG) bs=512 seek=1100 conv=notrunc 2>/dev/null \
		&& echo "[assets] assets.pak ecrit au secteur 1100"; \
	else \
		echo "AVERTISSEMENT : assets/assets.pak absent (python3 tools/pack_assets.py) -- fonds photo et sons desactives"; \
	fi



# ------------------------------------------------------------
# Disque de donnees (systeme de fichiers)
#
# Fichier separe de l'ISO de demarrage : filesystem/disk.c
# pilote desormais un vrai disque ATA (esclave, bus
# primaire) au lieu d'un tableau en RAM. Ce disque doit
# rester distinct de l'image de boot (maitre), sinon
# ecrire dessus corromprait le secteur de boot / le noyau.
# ------------------------------------------------------------


DISK_IMG = $(BUILD)/disk.img


$(DISK_IMG):

	mkdir -p $(BUILD)

	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=1



# ------------------------------------------------------------
# Run QEMU
# ------------------------------------------------------------


run: $(IMG) $(DISK_IMG)

	# Correctif (son de demarrage/extinction, kernel/drivers/
	# soundblaster) : sans "-device sb16", QEMU n'expose AUCUNE
	# carte son au noyau -- sb16_init() (kernel/kernel.c) echoue
	# alors sa detection (comportement attendu et gere : repli
	# silencieux sur le PC Speaker, voir kernel/drivers/speaker),
	# et le vrai son embarque n'est jamais joue. "pcspk-audiodev"
	# route aussi le PC Speaker de repli vers le meme backend audio
	# hote (QEMU recent le rend sinon inaudible par defaut, meme
	# comportement que sur la plupart des PC physiques actuels dont
	# le haut-parleur interne a disparu).

	qemu-system-x86_64 \
	-m 256M \
	-cpu max \
	-vga std \
	-nic user,model=$(QEMU_NIC) \
	-drive format=raw,file=$(IMG) \
	-drive format=raw,file=$(DISK_IMG) \
	-audiodev $(QEMU_AUDIODEV),id=snd0 \
	-machine pcspk-audiodev=snd0 \
	-device sb16,audiodev=snd0

# JARVIS (IA locale) : meme machine + COM1 expose sur TCP 127.0.0.1:4555.
# Lancer d'abord :  python3 tools/jarvis_bridge.py
run-ai: $(IMG) $(DISK_IMG)

	qemu-system-x86_64 \
	-m 256M \
	-cpu max \
	-vga std \
	-nic user,model=$(QEMU_NIC) \
	-drive format=raw,file=$(IMG) \
	-drive format=raw,file=$(DISK_IMG) \
	-audiodev $(QEMU_AUDIODEV),id=snd0 \
	-machine pcspk-audiodev=snd0 \
	-device sb16,audiodev=snd0 \
	-serial tcp:127.0.0.1:4555,server,nowait




# ------------------------------------------------------------
# Clean
# ------------------------------------------------------------


clean:

	rm -rf $(BUILD)



-include $(C_OBJECTS:.o=.d)

.PHONY: all clean run run-ai dirs
