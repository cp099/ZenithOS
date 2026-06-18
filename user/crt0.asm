; ==========================================
; ZenithOS Userland Runtime Entry (crt0)
; Calls main and invokes exit syscall
; ==========================================

[bits 32]

global _start
extern main

_start:
    ; Execute program main entry point
    mov eax, [esp + 4]    ; argc
    mov ebx, [esp + 8]    ; argv
    push ebx              ; argv
    push eax              ; argc
    call main

    ; Program finished, swap buffers first to flush final output
    mov eax, 16
    int 0x80

    ; Call SYS_EXIT syscall with return status
    mov ebx, eax          ; Exit code returned by main
    mov eax, 3            ; SYS_EXIT is syscall index 3
    int 0x80        ; Trigger syscall gate

    ; Fallback hang if syscall fails
.hang:
    hlt
    jmp .hang
