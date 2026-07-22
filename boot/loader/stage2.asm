[BITS 16]

[ORG 0x8000]


start:


cli


lgdt [gdt_descriptor]



; Activer A20

in al,0x92

or al,00000010b

out 0x92,al



; Mode protégé

mov eax,cr0

or eax,1

mov cr0,eax



jmp CODE32_SEL:protected



; ==========================
; 32 BIT MODE
; ==========================


[BITS 32]


protected:


mov ax,DATA32_SEL

mov ds,ax
mov ss,ax



; Activer PAE

mov eax,cr4

or eax,1<<5

mov cr4,eax



; Charger Page Tables

mov eax,page_table

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



extern _start


call _start



halt:

hlt

jmp halt



; ==========================
; PAGE TABLES
; ==========================


align 4096


page_table:

dq 0x0000000000000003



times 511 dq 0



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