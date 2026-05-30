; ==========================================
; ZenithOS Userland Runtime Entry (crt0)
; Calls main and invokes exit syscall
; ==========================================

[bits 32]

global _start
extern main

_start:
    ; Execute program main entry point
    call main

    ; Program finished, call SYS_EXIT syscall
    mov eax, 3      ; SYS_EXIT is syscall index 3
    int 0x80        ; Trigger syscall gate

    ; Fallback hang if syscall fails
.hang:
    hlt
    jmp .hang
