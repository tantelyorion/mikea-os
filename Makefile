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

CC ?= gcc

LD ?= ld

OBJCOPY ?= objcopy


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
# un compilateur croise "x86_64-elf-gcc" dedie. TARGET_FLAG est
# vide par defaut (inutile avec gcc/clang natif sur un hote deja
# x86_64) ; a renseigner uniquement avec Clang sur un hote d'une
# autre architecture, ex. :
#   make CC=clang LD=ld.lld OBJCOPY=llvm-objcopy \
#        TARGET_FLAG=--target=x86_64-elf

TARGET_FLAG ?=



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
-c


LD_FLAGS = \
-T linker.ld \
-z max-page-size=0x1000



# ------------------------------------------------------------
# Directories
# ------------------------------------------------------------


BUILD = build

OBJDIR = $(BUILD)/obj


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
C_SOURCES = $(shell find kernel filesystem shell security packages mkx libc gui -name "*.c" 2>/dev/null)

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


$(BOOT_BIN):

	$(ASM) \
	$(BOOT)/boot.asm \
	-f bin \
	-o $(BOOT_BIN)



# ------------------------------------------------------------
# Stage 2 Loader
# ------------------------------------------------------------


$(STAGE2_BIN):

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



# ------------------------------------------------------------
# Create disk image (image disque brute, pas une vraie ISO9660)
# ------------------------------------------------------------


$(IMG): $(BOOT_BIN) $(STAGE2_BIN) $(KERNEL_BIN)

	cat \
	$(BOOT_BIN) \
	$(STAGE2_BIN) \
	$(KERNEL_BIN) \
	> $(IMG)

	# Correctif critique (lecture disque du noyau echoue) :
	# boot.asm+stage2.asm+kernel.bin ne remplit ce fichier que
	# jusqu'a sa taille exacte (en general a peine plus que
	# 49664 + quelques dizaines de Ko), alors que dap_kernel
	# (boot/loader/stage2.asm) demande TOUJOURS 600 secteurs
	# (300 Ko) a partir du LBA 97, quelle que soit la taille
	# reelle du noyau (marge volontairement large, voir le
	# commentaire de dap_kernel). Sans ce correctif, ce fichier
	# etait donc systematiquement plus court que ce que la
	# lecture demande : QEMU/le BIOS refusent de lire au-dela
	# de la fin reelle du fichier, INT13h AH=0x42 renvoie une
	# erreur (drapeau carry), et stage2 affiche "Erreur de
	# lecture disque (noyau)" avant de figer la machine -- le
	# noyau n'etait jamais charge, meme quand la compilation
	# reussissait parfaitement. On complete donc ce fichier a
	# 1 Mo (tres largement au-dessus des 340480 octets requis,
	# de quoi laisser une bonne marge de croissance au noyau).

	truncate -s 1M $(IMG)



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

	qemu-system-x86_64 \
	-drive format=raw,file=$(IMG) \
	-drive format=raw,file=$(DISK_IMG)



# ------------------------------------------------------------
# Clean
# ------------------------------------------------------------


clean:

	rm -rf $(BUILD)



.PHONY: all clean run dirs
