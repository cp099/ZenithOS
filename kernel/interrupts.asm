; ==========================================
; ZenithOS Low-Level Interrupt Handler Stubs
; Handles Register pushes, stack frame creation, C-linking, and returning.
; ==========================================

[bits 32]

extern handle_interrupt

global isr_common_stub

; Common stub that builds the registers_t structure and routes to C handler
isr_common_stub:
    pusha                   ; Pushes edi, esi, ebp, esp, ebx, edx, ecx, eax

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10            ; Load the kernel data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                ; Push ESP (pointer to registers_t struct)
    call handle_interrupt
    add esp, 4              ; Clean parameter

    pop gs
    pop fs
    pop es
    pop ds

    popa                    ; Pops edi, esi, ebp, ...
    add esp, 8              ; Cleans up the pushed error code and pushed ISR number
    iret                    ; Return from interrupt (enables interrupts if they were enabled)

; Macro for exceptions that do NOT push an error code (we push dummy 0)
%macro ISR_NOERRCODE 1
    global isr%1
    isr%1:
        push byte 0         ; Dummy error code
        push %1             ; Interrupt number
        jmp isr_common_stub
%endmacro

; Macro for exceptions that DO push an error code
%macro ISR_ERRCODE 1
    global isr%1
    isr%1:
        push %1             ; Interrupt number
        jmp isr_common_stub
%endmacro

; Macro for hardware interrupts (IRQs, map to 32 + %1, push dummy 0)
%macro IRQ 2
    global irq%1
    irq%1:
        push byte 0         ; Dummy error code
        push %2             ; Target interrupt index
        jmp isr_common_stub
%endmacro

; 1. CPU Exception Entries
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30
ISR_NOERRCODE 31

; 2. Hardware IRQ Entries (Mapped to Interrupts 32-47)
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
; IRQ 11 entry point mapped as idt_irq11 to avoid assembly name collisions
global idt_irq11
idt_irq11:
    push byte 0
    push 43
    jmp isr_common_stub

IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; 3. Syscall Gate entry (interrupt 128 = 0x80)
global isr128
isr128:
    push byte 0             ; Dummy error code
    push 128                ; Interrupt number
    jmp isr_common_stub

; 4. Task Context Switcher
global switch_context
switch_context:
    push ebp
    push ebx
    push esi
    push edi
    
    mov eax, [esp + 20]
    mov [eax], esp        ; Save current ESP to *old_esp
    
    mov esp, [esp + 24]   ; Load new ESP
    
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
