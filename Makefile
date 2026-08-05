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

CC = x86_64-elf-gcc

LD = x86_64-elf-ld

OBJCOPY = x86_64-elf-objcopy



# ------------------------------------------------------------
# Flags
# ------------------------------------------------------------


ASM_FLAGS = -f elf64


C_FLAGS = \
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


ISO = $(BUILD)/MikeaOS.iso



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


all: clean dirs $(ISO)



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
# Create ISO image
# ------------------------------------------------------------


$(ISO): $(BOOT_BIN) $(STAGE2_BIN) $(KERNEL_BIN)

	cat \
	$(BOOT_BIN) \
	$(STAGE2_BIN) \
	$(KERNEL_BIN) \
	> $(ISO)



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


run: $(ISO) $(DISK_IMG)

	qemu-system-x86_64 \
	-drive format=raw,file=$(ISO) \
	-drive format=raw,file=$(DISK_IMG)



# ------------------------------------------------------------
# Clean
# ------------------------------------------------------------


clean:

	rm -rf $(BUILD)



.PHONY: all clean run dirs
