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


    mov si, msg


print:

    lodsb

    cmp al,0
    je load_stage2


    mov ah,0x0E
    int 0x10

    jmp print



load_stage2:

    ; Charger stage2 depuis disque
    mov bx,0x8000

    mov ah,0x02

    ; Correctif : stage2 contient une table de pages alignee
    ; sur 4096 octets (voir boot/loader/stage2.asm) : sa taille
    ; reelle depasse largement les 4 secteurs (2048 octets)
    ; lus par l'ancienne version. On lit desormais 32 secteurs
    ; (16 Ko) par securite. A ajuster si stage2.bin depasse un
    ; jour cette taille (verifier avec `ls -la build/stage2.bin`
    ; apres compilation).
    mov al,32

    mov ch,0
    mov dh,0
    mov dl,[boot_drive]
    mov cl,2

    int 0x13


    jmp 0x0000:0x8000



boot_drive:

db 0


msg:

db "Mikea OS Bootloader",13,10
db "Loading Kernel...",13,10
db 0


times 510-($-$$) db 0

dw 0xAA55