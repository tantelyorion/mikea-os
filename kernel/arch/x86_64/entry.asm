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


extern kernel_start



section .text



; ============================================================
; Kernel Entry Point
; ============================================================


_start:


    cli


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


    lea rsp,[stack_top]


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