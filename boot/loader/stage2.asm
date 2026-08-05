[BITS 16]

[ORG 0x8000]


start:


cli


; Correctif : on sauvegarde le numero de disque de demarrage
; (transmis par le BIOS dans DL, et deja preserve par
; boot.asm jusqu'ici) des l'entree dans stage2, avant de s'en
; servir plus bas pour charger le noyau.

mov [boot_drive], dl


lgdt [gdt_descriptor]



; Activer A20

in al,0x92

or al,00000010b

out 0x92,al



; ============================================================
; Charger le noyau (kernel.bin) depuis le disque
; ============================================================
;
; Correctif majeur : rien, nulle part, ne chargeait
; auparavant le noyau en memoire. Ce fichier se contentait de
; faire "extern _start" / "call _start", en esperant sauter
; dans le code du noyau -- une instruction depourvue de sens
; pour un fichier assemble en binaire brut (voir le Makefile,
; regle $(STAGE2_BIN) : "-f bin", sans edition de liens) :
; NASM n'a aucun moyen de connaitre l'adresse de "_start" dans
; ce contexte, et meme si l'assemblage avait reussi, aucun
; octet du noyau n'aurait de toute facon ete present en
; memoire a cet endroit.
;
; On charge ici kernel.bin dans un tampon temporaire bas
; (0x10000, directement adressable en mode reel 16 bits),
; avant de le recopier vers son adresse definitive de
; chargement (1 Mo, voir linker.ld : KERNEL_BASE = 0x00100000)
; une fois en mode protege 32 bits a adressage plat -- 1 Mo
; n'est pas atteignable avec un simple ES:BX 16 bits en mode
; reel standard (voir plus bas, section "protected:").
;
; kernel.bin commence au secteur logique (LBA) 65 du disque :
; LBA 0 = le secteur de demarrage (boot.asm), LBA 1-64 =
; stage2.bin (complete pour occuper exactement ces 64
; secteurs, voir le "times" final de ce fichier, afin que cet
; emplacement soit fixe quelle que soit la taille reelle du
; code de stage2). 600 secteurs (300 Ko) de marge sont lus,
; largement suffisant pour ce noyau.

mov si,dap_kernel

mov ah,0x42
mov dl,[boot_drive]

int 0x13

jc kernel_disk_error



; Mode protégé

mov eax,cr0

or eax,1

mov cr0,eax


jmp CODE32_SEL:protected



kernel_disk_error:

; Impossible de continuer sans le noyau : on fige la machine
; plutot que de sauter dans une zone memoire non initialisee,
; ce qui provoquerait un comportement totalement imprevisible.

cli

.khalt:

hlt

jmp .khalt



boot_drive:

db 0


; ------------------------------------------------------------
; Disk Address Packet (DAP) pour la lecture LBA du noyau
; ------------------------------------------------------------

dap_kernel:

db 0x10
db 0
dw 600
dw 0x0000
dw 0x1000
dq 65



; ==========================
; 32 BIT MODE
; ==========================


[BITS 32]


protected:


mov ax,DATA32_SEL

mov ds,ax
mov ss,ax
mov es,ax



; ------------------------------------------------------------
; Recopier le noyau vers son adresse definitive (1 Mo)
; ------------------------------------------------------------
;
; On dispose desormais d'un adressage plat 32 bits complet
; (descripteurs CODE32/DATA32 : base 0, limite 4 Go), donc
; plus besoin des approximations segment:offset du mode reel
; 16 bits pour atteindre une adresse au-dela de 1 Mo.

mov esi,0x10000

mov edi,0x100000

mov ecx,(600*512)/4

cld

rep movsd



; Activer PAE

mov eax,cr4

or eax,1<<5

mov cr4,eax



; Charger Page Tables

mov eax,pml4

mov cr3,eax



; Activer Long Mode

mov ecx,0xC0000080

rdmsr


or eax,1<<8


wrmsr



; Activer paging

mov eax,cr0

or eax,1<<31


mov cr0,eax



jmp CODE64_SEL:long_mode



; ==========================
; 64 BIT MODE
; ==========================


[BITS 64]


long_mode:


mov ax,DATA64_SEL


mov ds,ax
mov ss,ax



; Correctif : "extern _start" / "call _start" ne peut pas
; fonctionner ici (voir le commentaire plus haut) -- on saute
; directement a l'adresse physique fixe ou linker.ld place le
; point d'entree du noyau (KERNEL_BASE = 0x00100000, le tout
; premier objet lie -- kernel/arch/x86_64/entry.asm -- y place
; "_start" en tout premier).

mov rax,0x100000

call rax



halt:

hlt

jmp halt



; ==========================
; PAGE TABLES
; ==========================
;
; Correctif critique : l'ancienne version definissait UNE
; SEULE table de 512 entrees ("page_table"), avec une seule
; entree non nulle (0x0000000000000003 = present + inscriptible,
; adresse de base 0) chargee directement dans CR3.
;
; Ce n'est pas ainsi que fonctionne la pagination x86-64 : CR3
; pointe toujours vers une table PML4 (niveau 4), dont chaque
; entree pointe vers une table PDPT (niveau 3), dont chaque
; entree pointe elle-meme vers une table PD (niveau 2) -- ou
; l'on peut mapper directement des pages de 2 Mo via le bit PS.
; L'ancienne entree PML4[0] = 0x3 disait donc au CPU "la table
; PDPT suivante se trouve a l'adresse physique 0", une adresse
; qui ne contient aucune table PDPT valide (au mieux l'IVT du
; BIOS, au pire du code). Des l'activation de la pagination
; (CR0.PG, quelques lignes plus haut), le CPU aurait presque
; certainement declenche une exception de page fault non geree
; (aucune IDT n'est encore chargee a ce stade du demarrage)
; puis un triple fault, redemarrant la machine sans aucun
; message d'erreur.
;
; On construit ici une vraie hierarchie a 3 niveaux (PML4 ->
; PDPT -> PD) avec des pages de 2 Mo (bit PS) : toujours
; disponibles en mode long, contrairement aux pages de 1 Go qui
; dependent d'une fonctionnalite CPU optionnelle (PDPE1GB) non
; garantie sur tout processeur x86-64. 32 entrees PD suffisent
; a mapper en identite (adresse physique = adresse virtuelle)
; les 64 premiers Mo de RAM physique -- largement de quoi
; couvrir le noyau (charge a 1 Mo, voir linker.ld KERNEL_BASE),
; son tas (kernel/memory/heap.c) et sa pile.

align 4096

pml4:

dq pdpt + 0x3

times 511 dq 0


align 4096

pdpt:

dq pd + 0x3

times 511 dq 0


align 4096

pd:

%assign pd_index 0

%rep 32

dq (pd_index * 0x200000) | 0x83

%assign pd_index pd_index+1

%endrep

times (512-32) dq 0



; ==========================
; GDT
; ==========================

; Correctif : le code plus haut utilisait les etiquettes
; CODE32/DATA32/CODE64/DATA64 directement comme selecteur
; de segment. Une etiquette vaut son ADRESSE (0x8000 + son
; decalage, a cause de [ORG 0x8000]), pas son DECALAGE DANS
; LA GDT : un vrai selecteur de segment doit etre un petit
; index (0x08, 0x10, 0x18, 0x20...), pas une adresse memoire.
; Sans ce correctif, la transition vers le mode protege
; chargerait un selecteur invalide et provoquerait un
; #GP / triple fault immediat.

CODE32_SEL equ CODE32 - gdt
DATA32_SEL equ DATA32 - gdt
CODE64_SEL equ CODE64 - gdt
DATA64_SEL equ DATA64 - gdt


gdt:


dq 0



CODE32:

dw 0xffff

dw 0

db 0

db 10011010b

db 11001111b

db 0



DATA32:

dw 0xffff

dw 0

db 0

db 10010010b

db 11001111b

db 0



CODE64:

dw 0

dw 0

db 0

db 10011010b

db 00100000b

db 0



DATA64:

dw 0

dw 0

db 0

db 10010010b

db 00000000b

db 0



gdt_descriptor:


dw gdt_descriptor-gdt-1

dd gdt



; ==========================
; PADDING
; ==========================
;
; Complete stage2.bin jusqu'a occuper exactement 64 secteurs
; (32 768 octets), quelle que soit la taille reelle du code
; ci-dessus. Indispensable pour que kernel.bin commence
; toujours a un emplacement disque fixe et connu (LBA 65, voir
; "dap_kernel" plus haut) : boot.asm et stage2.asm determinent
; cet emplacement par un decalage constant plutot qu'en lisant
; la taille reelle de stage2.bin sur le disque.

times 32768-($-$$) db 0
