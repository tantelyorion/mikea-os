[BITS 16]

[ORG 0x8000]


start:


cli


; Correctif : on sauvegarde le numero de disque de demarrage
; (transmis par le BIOS dans DL, et deja preserve par
; boot.asm jusqu'ici) des l'entree dans stage2, avant de s'en
; servir plus bas pour charger le noyau.

mov [boot_drive], dl


; Point de controle de diagnostic (correctif) : confirme que
; stage2 a bien ete atteint et execute (mode reel, donc via
; INT10h comme boot.asm). Si l'ecran s'arrete avant ce
; message alors que boot.asm affichait deja "[2/2] Stage2
; charge, saut...", le probleme vient du saut lui-meme
; (jmp 0x0000:0x8000) ou de l'execution des tout premiers
; octets de stage2.bin -- verifiez que le fichier .img n'a
; pas ete tronque/corrompu lors de sa copie.

mov si,msg_stage2_start
call print_string_rm


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
;
; Correctif critique (echouait TOUJOURS, quelle que soit la
; taille du fichier .img) : de tres nombreux BIOS -- dont
; SeaBIOS, le firmware utilise par QEMU, la cible principale
; de ce projet -- limitent CHAQUE appel INT13h AH=0x42 a 127
; secteurs au maximum (voir la Disk Address Packet : le champ
; "nombre de secteurs" fait 16 bits, mais la limite reelle
; imposee par le BIOS lui-meme est bien plus basse). Demander
; 600 secteurs en un seul appel echoue donc systematiquement
; avec une erreur "parametre invalide" (drapeau carry), peu
; importe que le fichier disque sous-jacent soit assez grand
; ou non -- ce n'est pas une question de taille de fichier
; mais de taille de CHAQUE requete individuelle.
;
; On decoupe donc la lecture en 6 appels de 100 secteurs
; chacun (6*100 = 600, et 100 reste bien en dessous de la
; limite de 127), en avancant a chaque iteration le LBA de
; depart et le segment de destination (l'offset reste
; toujours 0 : chaque bloc de 100 secteurs = 51200 octets =
; 0xC800, qui tient entierement dans un segment 16 bits sans
; le depasser, ce qui evite tout probleme de franchissement
; de limite de segment).

mov cx,6

.read_kernel_chunk:

mov si,dap_kernel

mov ah,0x42
mov dl,[boot_drive]

int 0x13

jc kernel_disk_error


add word [dap_kernel_seg],0xC80

add dword [dap_kernel_lba],100


dec cx

jnz .read_kernel_chunk


; Point de controle de diagnostic (correctif) : si l'ecran
; s'arrete entre "[stage2] demarre" et ce message, la lecture
; disque du noyau echoue encore -- voir le correctif documente
; plus haut (limite de 127 secteurs par appel INT13h AH=0x42).
; Verifiez aussi, au cas ou, que l'image .img fait bien au
; moins 65+600 secteurs (33280+307200 octets).

mov si,msg_kernel_loaded
call print_string_rm



; Mode protégé

mov eax,cr0

or eax,1

mov cr0,eax


jmp CODE32_SEL:protected



kernel_disk_error:

; Impossible de continuer sans le noyau : on fige la machine
; plutot que de sauter dans une zone memoire non initialisee,
; ce qui provoquerait un comportement totalement imprevisible.

mov si,msg_kernel_disk_error
call print_string_rm

cli

.khalt:

hlt

jmp .khalt



; ------------------------------------------------------------
; Affichage texte en mode reel (16 bits), via BIOS INT10h --
; utilisable uniquement avant le passage en mode protege plus
; bas (les interruptions BIOS ne fonctionnent plus ensuite).
; ------------------------------------------------------------

print_string_rm:

.next:

lodsb

cmp al,0

je .done

mov ah,0x0E

int 0x10

jmp .next

.done:

ret



msg_stage2_start:

db "[stage2] demarre",13,10
db 0


msg_kernel_loaded:

db "[stage2] noyau charge, passage 32 bits...",13,10
db 0


msg_kernel_disk_error:

db "[stage2] Erreur de lecture disque (noyau)",13,10
db 0



boot_drive:

db 0


; ------------------------------------------------------------
; Disk Address Packet (DAP) pour la lecture LBA du noyau
; ------------------------------------------------------------
;
; "dw 100" (et non 600) : voir le correctif documente plus
; haut, chaque appel INT13h AH=0x42 est limite a 127 secteurs
; par la plupart des BIOS (dont SeaBIOS/QEMU) -- 100 par appel,
; repete 6 fois par la boucle ci-dessus, reste bien en dessous
; de cette limite. dap_kernel_seg et dap_kernel_lba sont
; modifies directement en memoire a chaque iteration (ce ne
; sont pas des constantes figees a l'assemblage).

dap_kernel:

db 0x10
db 0
dw 100
dw 0x0000

dap_kernel_seg:

dw 0x1000

dap_kernel_lba:

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


; Point de controle de diagnostic (correctif) : premiere
; ecriture possible une fois en mode protege -- les
; interruptions BIOS (int 0x10) ne fonctionnent plus ici, on
; ecrit donc directement dans la memoire video VGA texte
; (0xB8000), sur la ligne 5 pour ne pas ecraser les messages
; deja affiches en mode reel plus haut sur l'ecran. Si rien
; n'apparait sur cette ligne alors que "[stage2] noyau
; charge..." s'affichait juste avant, le blocage vient du
; saut "jmp CODE32_SEL:protected" ou de l'activation du mode
; protege (mov cr0) elle-meme -- verifiez le contenu du GDT
; (CODE32_SEL/DATA32_SEL).

mov esi,msg_protected_ok
mov edi,0xB8000 + (80*2*5)
call print_vga32



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


; Point de controle de diagnostic (correctif) : juste apres
; l'activation de la pagination (ligne 6). Si l'ecran
; s'arrete entre "Mode protege OK" (ligne 5) et ce point,
; le probleme vient de la copie du noyau (rep movsd) ou de
; la configuration PAE/pagination/MSR EFER ci-dessus -- une
; mauvaise entree de table de pages declenche typiquement un
; triple fault (redemarrage immediat de la VM, ecran qui
; "clignote" et recommence a "Booting from hard disk...").

mov esi,msg_paging_ok
mov edi,0xB8000 + (80*2*6)
call print_vga32


jmp CODE64_SEL:long_mode



; ------------------------------------------------------------
; Affichage texte en mode protege 32 bits, ecriture directe
; dans la memoire video VGA (0xB8000) puisque les
; interruptions BIOS ne sont plus utilisables ici.
;   ESI = chaine terminee par 0
;   EDI = adresse VGA de destination (0xB8000 + ligne*160)
; ------------------------------------------------------------

print_vga32:

mov ah,0x0F

.loop:

lodsb

cmp al,0

je .done

mov [edi],al
mov [edi+1],ah

add edi,2

jmp .loop

.done:

ret



msg_protected_ok:

db "[stage2] Mode protege OK",0


msg_paging_ok:

db "[stage2] Pagination OK, passage 64 bits...",0



; ==========================
; 64 BIT MODE
; ==========================


[BITS 64]


long_mode:


mov ax,DATA64_SEL


mov ds,ax
mov ss,ax


; Point de controle de diagnostic (correctif) : dernier point
; avant de sauter dans le noyau C (ligne 7). Ecriture VGA
; directe, comme en mode protege 32 bits juste avant (meme
; principe, mais registres 64 bits). Si ce message s'affiche
; mais que rien ne se passe ensuite (pas de "Default account"
; ni d'ecran de connexion, voir kernel/kernel.c), le probleme
; n'est PAS dans le demarrage bas niveau mais dans le noyau
; C lui-meme -- verifiez que le noyau a bien ete recopie en
; entier a 0x100000 (rep movsd plus haut) et que son point
; d'entree _start (kernel/arch/x86_64/entry.asm) initialise
; correctement la pile avant d'appeler kernel_start().

mov rsi,msg_kernel_call
mov rdi,0xB8000 + (80*2*7)
mov ah,0x0F

.print_loop:

lodsb

cmp al,0

je .print_done

mov [rdi],al
mov [rdi+1],ah

add rdi,2

jmp .print_loop

.print_done:


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



msg_kernel_call:

db "[stage2] Appel du noyau...",0



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
