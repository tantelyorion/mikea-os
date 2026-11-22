; ============================================================
;
;              Mikea OS Kernel Entry
;
;              Architecture : x86_64
;              Language    : Assembly (NASM)
;
;              Developer :
;              Tantely Orion
;
;              Version :
;              Mikea OS 0.3.0
;
; ============================================================


[BITS 64]


global _start
extern __bss_start
extern __bss_end


extern kernel_start



section .text



; ============================================================
; Kernel Entry Point
; ============================================================


_start:

    cli

    ; Correctif (affichage/execution sous QEMU) : kernel.bin est
    ; produit par "objcopy -O binary", qui N'INCLUT PAS la section
    ; .bss (NOBITS). Rien n'initialisait donc a zero les variables
    ; statiques non initialisees (curseur, drapeaux, etats des
    ; pilotes...) : elles contenaient ce que la RAM contenait deja
    ; a cet endroit. On efface le BSS ici, AVANT de configurer la
    ; pile (qui est elle-meme dans le BSS).

    lea rdi,[rel __bss_start]
    lea rcx,[rel __bss_end]
    sub rcx,rdi
    xor eax,eax
    cld
    rep stosb

    ; --------------------------------------------------------
    ; Initialisation des segments
    ; --------------------------------------------------------


    mov ax,0x10

    mov ds,ax
    mov es,ax
    mov ss,ax



    ; --------------------------------------------------------
    ; Initialiser la pile Kernel
    ; --------------------------------------------------------


    lea rsp,[rel stack_top]


    ; Alignement 16 bytes obligatoire ABI x86_64

    and rsp,-16



    ; --------------------------------------------------------
    ; Effacer direction flag
    ; --------------------------------------------------------


    cld



    ; --------------------------------------------------------
    ; Appel du kernel C
    ;
    ; Fonction :
    ;
    ; void kernel_start();
    ;
    ; --------------------------------------------------------


    call kernel_start



; ============================================================
; Si le kernel retourne
; ============================================================


kernel_halt:


    cli


.halt_loop:


    hlt

    jmp .halt_loop






; ============================================================
; Kernel Stack
; ============================================================


section .bss



align 16



stack_bottom:


resb 16384


stack_top: