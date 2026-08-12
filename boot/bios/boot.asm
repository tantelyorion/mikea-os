[BITS 16]
[ORG 0x7C00]


start:

    cli

    ; Correctif : le BIOS transmet le numero du disque de
    ; demarrage dans DL au demarrage. L'ancienne version
    ; ecrasait DL avec 0x00 code en dur avant l'appel a
    ; INT 13h, ce qui ne fonctionne que si l'on demarre
    ; effectivement depuis le premier disque (0x00) -- pas
    ; le cas si QEMU/le BIOS demarre depuis un disque dur
    ; ou une image CD (souvent 0x80 ou 0xE0+). On sauvegarde
    ; donc DL ici, avant qu'il ne soit modifie par autre chose.
    mov [boot_drive], dl

    xor ax, ax
    mov ds, ax
    mov es, ax


    mov si, msg


print:

    lodsb

    cmp al,0
    je check_extensions


    mov ah,0x0E
    int 0x10

    jmp print



check_extensions:

    ; Correctif majeur : ce secteur de demarrage utilisait
    ; auparavant une lecture disque CHS classique (INT13h
    ; AH=0x02) avec un nombre de secteurs (32 puis 64)
    ; largement superieur a la limite de 63 secteurs par
    ; piste que le registre CL peut exprimer pour un
    ; adressage CHS -- et sans jamais tenir compte de la
    ; geometrie reelle du disque (tetes, cylindres). On
    ; bascule ici sur les extensions BIOS INT13h (lecture par
    ; LBA, adresse de secteur logique unique de bout en bout),
    ; qui eliminent completement ce probleme et sont
    ; supportees par la quasi-totalite des BIOS modernes,
    ; dont SeaBIOS (utilise par QEMU, cible de ce projet).
    ;
    ; On verifie d'abord leur presence (fonction 0x41) avant
    ; de s'en servir.

    mov ah,0x41
    mov bx,0x55AA
    mov dl,[boot_drive]
    int 0x13

    jc no_extensions

    cmp bx,0xAA55
    jne no_extensions


    ; Point de controle de diagnostic (correctif) : si l'ecran
    ; s'arrete avant ce point, le blocage vient de l'appel
    ; INT13h AH=0x41 lui-meme (verification des extensions),
    ; donc du BIOS/de l'emulation plutot que de notre code.
    mov si,msg_ext_ok
    call print_string


load_stage2:

    ; Charge stage2.bin par LBA : secteur logique 1 (le
    ; secteur logique 0 est ce secteur de demarrage
    ; lui-meme), sur 96 secteurs (48 Ko), a l'adresse
    ; 0x0000:0x8000.
    ;
    ; stage2.asm est complete (voir la fin de ce fichier
    ; -- pardon, de stage2.asm) jusqu'a occuper exactement
    ; ces 96 secteurs quelle que soit la taille reelle de son
    ; code, afin que kernel.bin commence toujours a un
    ; emplacement disque fixe et connu (LBA 97). C'est
    ; stage2.asm qui se charge ensuite de lire kernel.bin --
    ; ce secteur de demarrage ne s'occupe que de stage2.

    mov si,dap_stage2

    mov ah,0x42
    mov dl,[boot_drive]

    int 0x13

    jc disk_error


    ; Point de controle de diagnostic (correctif) : si l'ecran
    ; s'arrete entre "Extensions OK" et ce message, le
    ; blocage vient de la lecture disque de stage2 elle-meme
    ; (INT13h AH=0x42) -- verifiez que le premier "-drive"
    ; QEMU pointe bien sur build/MikeaOS.img avec
    ; "format=raw" (jamais "-cdrom"), ou que le fichier est
    ; bien attache comme DISQUE DUR (pas lecteur optique)
    ; dans VirtualBox.
    mov si,msg_stage2_ok
    call print_string


    jmp 0x0000:0x8000



no_extensions:

    mov si,msg_no_ext

    call print_string

    jmp halt



disk_error:

    mov si,msg_disk_error

    call print_string

    jmp halt



halt:

    cli

.loop:

    hlt

    jmp .loop



print_string:

.next:

    lodsb

    cmp al,0

    je .done

    mov ah,0x0E

    int 0x10

    jmp .next

.done:

    ret



boot_drive:

db 0


; ------------------------------------------------------------
; Disk Address Packet (DAP) pour la lecture LBA de stage2.bin
; ------------------------------------------------------------
;
; Format standard (16 octets) des extensions BIOS INT13h :
;   +0  taille du paquet (0x10)
;   +1  reserve (0)
;   +2  nombre de secteurs a transferer
;   +4  decalage (offset) du tampon destination
;   +6  segment du tampon destination
;   +8  secteur logique (LBA) de depart, sur 64 bits

; Correctif (marge de taille) : 96 secteurs (48 Ko) au lieu de
; 64 -- les tables de pages couvrent desormais 4 Go d'identity
; mapping (voir stage2.asm) au lieu de 64 Mo, ce qui ne rentrait
; plus confortablement dans les 32 Ko precedents. 96 reste tres
; en dessous de la limite de 127 secteurs par appel INT13h
; AH=0x42 (voir le correctif documente dans stage2.asm pour le
; chargement du noyau, qui lui doit se decouper en plusieurs
; appels a cause de cette meme limite -- stage2 tient ici en un
; seul appel).

dap_stage2:

db 0x10
db 0
dw 96
dw 0x8000
dw 0x0000
dq 1


msg:

db "Mikea OS Bootloader",13,10
db "Loading Kernel...",13,10
db 0


msg_ext_ok:

db "[1/2] Extensions BIOS OK",13,10
db 0


msg_stage2_ok:

db "[2/2] Stage2 charge, saut...",13,10
db 0


msg_no_ext:

db "Erreur : extensions BIOS (LBA) absentes",13,10
db 0


msg_disk_error:

db "Erreur de lecture disque (stage2)",13,10
db 0


times 510-($-$$) db 0

dw 0xAA55
