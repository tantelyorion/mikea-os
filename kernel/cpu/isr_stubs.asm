
; ============================================================
;
;              Mikea OS - ISR / IRQ stubs
;
;              Points d'entree bas niveau de toutes les
;              interruptions (exceptions CPU 0-31 et IRQ
;              materielles remappees 32-47). Chaque stub
;              sauvegarde le numero de vecteur (et un faux
;              code d'erreur pour les exceptions qui n'en
;              fournissent pas), puis saute vers une routine
;              commune qui sauvegarde les registres et
;              appelle le gestionnaire C correspondant.
;
; ============================================================


[BITS 64]


extern isr_handler

extern irq_handler


; Nombre d'octets pousses par isr_common_stub/irq_common_stub
; pour les registres generaux (15 registres x 8 octets).

REGS_SIZE equ 15*8



%macro ISR_NOERR 1

global isr%1

isr%1:

    push qword 0

    push qword %1

    jmp isr_common_stub

%endmacro



%macro ISR_ERR 1

global isr%1

isr%1:

    push qword %1

    jmp isr_common_stub

%endmacro



%macro IRQ 2

global irq%1

irq%1:

    push qword 0

    push qword %2

    jmp irq_common_stub

%endmacro


section .text


; --------------------------------------------------------
; Exceptions CPU (0-31)
;
; Vecteurs qui poussent deja un code d'erreur materiel :
; 8, 10, 11, 12, 13, 14, 17, 21, 29, 30 (les autres non).
; --------------------------------------------------------

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31


; --------------------------------------------------------
; IRQ materielles (remappees sur les vecteurs 32-47 par
; pic_remap(), voir kernel/interrupt/pic.c)
; --------------------------------------------------------

IRQ 0,  32
IRQ 1,  33
IRQ 2,  34
IRQ 3,  35
IRQ 4,  36
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47



; --------------------------------------------------------
; Routine commune : exceptions
;
; A l'entree, la pile contient (du sommet vers le fond) :
; [vecteur][code erreur][RIP][CS][RFLAGS]{[RSP][SS]}
; --------------------------------------------------------

isr_common_stub:

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, [rsp + REGS_SIZE]      ; argument 1 = numero de vecteur

    call isr_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16                     ; retire vecteur + code erreur

    iretq



; --------------------------------------------------------
; Routine commune : IRQ materielles
; --------------------------------------------------------

irq_common_stub:

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, [rsp + REGS_SIZE]      ; argument 1 = numero de vecteur (32-47)

    call irq_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16

    iretq
