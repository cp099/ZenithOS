; ==========================================
; ZenithOS Stage 1 Bootloader (MBR)
; Loads Stage 2 from the boot disk and jumps
; ==========================================

[org 0x7C00]
[bits 16]

start:
    ; Reset segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00              ; Stack grows downwards from 0x7C00

    ; Save the boot drive number passed by BIOS in DL
    mov [BOOT_DRIVE], dl

    ; Print greeting message
    mov si, GREETING_MSG
    call print_string

    ; Load Stage 2 from disk (sectors 2 to 16, LBA 1 to 15)
    mov dl, [BOOT_DRIVE]
    mov bx, 0x8000              ; Load target: 0x0000:0x8000
    mov dh, 0                   ; Head 0
    mov ch, 0                   ; Cylinder 0
    mov cl, 2                   ; Start sector (Sector 2 is right after MBR)
    mov al, 15                  ; Read 15 sectors (7.5KB space for Stage 2)
    call disk_read

    ; Print success and jump to Stage 2
    mov si, JUMPING_MSG
    call print_string

    ; Far jump to Stage 2
    jmp 0x0000:0x8000

; ------------------------------------------
; Functions
; ------------------------------------------

; Print null-terminated string at DS:SI using BIOS TTY
print_string:
    push ax
    push si
    mov ah, 0x0E                ; BIOS teletype output
.loop:
    lodsb                       ; Load AL from [SI], increment SI
    test al, al                 ; Check if AL is 0
    jz .done
    int 0x10                    ; Video interrupt
    jmp .loop
.done:
    pop si
    pop ax
    ret

; Read AL sectors into ES:BX from drive DL (CHS parameters: DH=head, CH=cylinder, CL=sector)
disk_read:
    push ax
    push bx
    push cx
    push dx

    mov ah, 0x02                ; BIOS read sectors function
    int 0x13
    jc .disk_error              ; Jump if carry flag set (error)

    pop dx
    pop cx
    pop bx
    pop ax
    ret

.disk_error:
    mov si, ERROR_MSG
    call print_string
    cli
    hlt

; ------------------------------------------
; Data
; ------------------------------------------

BOOT_DRIVE      db 0
GREETING_MSG    db "ZenithOS: Loading Stage 1...", 0x0D, 0x0A, 0
JUMPING_MSG     db "ZenithOS: Booting Stage 2...", 0x0D, 0x0A, 0
ERROR_MSG       db "ZenithOS: Disk Read Error!", 0x0D, 0x0A, 0

; ------------------------------------------
; Padding and Signature
; ------------------------------------------

times 510-($-$$) db 0           ; Pad remaining bytes with 0
dw 0xAA55                       ; Boot signature
