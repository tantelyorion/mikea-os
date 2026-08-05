
; ============================================================
;
;              Mikea OS - Context Switch
;
;              void context_switch(
;                  u64** old_sp_store,
;                  u64*  new_sp
;              );
;
;              rdi = adresse ou sauvegarder le rsp courant
;              rsi = valeur de rsp a charger (thread suivant)
;
;              System V AMD64 : rbp, rbx, r12-r15 sont
;              callee-saved, on les sauvegarde/restaure a la
;              main pour completer ce que le compilateur C
;              suppose deja preserve.
;
; ============================================================


[BITS 64]


global context_switch


section .text


context_switch:

    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov [rdi], rsp

    mov rsp, rsi

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ret
