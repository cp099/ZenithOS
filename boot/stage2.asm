; ==========================================
; ZenithOS Stage 2 Bootloader
; Prepares environment, enters Protected Mode, relocates and executes kernel
; ==========================================

[org 0x8000]
[bits 16]

stage2_start:
    ; Setup segment registers and stack for Stage 2
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00              ; Stack at 0x7C00 (grows down, safe area)

    ; Save the boot drive passed in DL by Stage 1
    mov [BOOT_DRIVE], dl

    ; Print greeting
    mov si, STAGE2_MSG
    call print_string_16

    ; Load Kernel Binary from disk
    ; Kernel starts at Sector 17 (LBA 16)
    ; We will load 128 sectors (64KB space) to memory 0x1000:0000 (physical 0x10000)
    mov si, LOAD_KERNEL_MSG
    call print_string_16

    mov cx, 256                 ; 256 sectors to read
    mov ax, 16                  ; Start LBA 16
    mov bx, 0x1000              ; Target segment
    mov es, bx
    xor bx, bx                  ; Target offset 0
    
.read_loop:
    push ax
    push cx
    push bx
    
    ; Convert LBA (in AX) to CHS
    call lba_to_chs
    
    ; Read 1 sector
    mov ax, 0x0201              ; AH=02 (read), AL=01 (1 sector)
    mov dl, [BOOT_DRIVE]
    int 0x13
    jnc .read_ok
    
    ; If failed, print error and halt
    mov si, DISK_ERR_MSG
    call print_string_16
    cli
    hlt

.read_ok:
    pop bx
    pop cx
    pop ax
    
    ; Advance target buffer address: ES:BX + 512 bytes
    add bx, 512
    jnz .no_segment_carry
    ; If BX overflowed 64KB, increment ES by 0x1000
    mov dx, es
    add dx, 0x1000
    mov es, dx

.no_segment_carry:
    inc ax                      ; Next LBA
    dec cx                      ; Decrement count
    jnz .read_loop

    ; Enable A20 Gate
    mov si, A20_MSG
    call print_string_16
    call enable_a20

    ; Set up VESA VBE Graphics Mode
    mov si, VESA_MSG
    call print_string_16

    xor ax, ax
    mov es, ax                  ; Ensure ES segment is 0

    ; Get Mode Information for Mode 0x411B (1280x1024 with linear framebuffer)
    mov ax, 0x4F01
    mov cx, 0x011B              ; Mode 0x11B (without linear framebuffer bit in query)
    mov di, 0x2000              ; ES:DI = 0x0000:0x2000 (temporary buffer)
    int 0x10
    cmp ax, 0x004F
    jne .vesa_error

    ; Write BootInfo structure at 0x7000
    mov eax, [0x2000 + 40]      ; Framebuffer physical pointer
    mov [0x7000], eax
    
    xor eax, eax
    mov ax, [0x2000 + 18]       ; Width
    mov [0x7004], eax
    
    xor eax, eax
    mov ax, [0x2000 + 20]       ; Height
    mov [0x7008], eax
    
    xor eax, eax
    mov ax, [0x2000 + 16]       ; Pitch (bytes per scanline)
    mov [0x700C], eax
    
    xor eax, eax
    mov al, [0x2000 + 25]       ; Bpp
    mov [0x7010], eax

    ; Set the video mode
    mov ax, 0x4F02
    mov bx, 0x411B              ; Mode 0x11B + Linear Frame Buffer (Bit 14 set)
    int 0x10
    cmp ax, 0x004F
    je .vesa_ok

.vesa_error:
    mov si, VESA_ERR_MSG
    call print_string_16
    cli
    hlt

.vesa_ok:
    ; Switch to Protected Mode
    mov si, PM_MSG
    call print_string_16

    cli                         ; Disable interrupts before switching PM
    lgdt [gdt_descriptor]       ; Load Global Descriptor Table

    ; Set protection enable bit in CR0
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to clear CPU pipeline and load CS with 32-bit segment descriptor
    jmp CODE_SEG:protected_mode_entry

; ------------------------------------------
; 16-Bit Real Mode Functions
; ------------------------------------------

; Print null-terminated string at DS:SI using BIOS TTY
print_string_16:
    push ax
    push si
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    pop si
    pop ax
    ret

; Convert LBA in AX to CHS parameters
; Output: CH = Cylinder, CL = Sector (1-based), DH = Head
lba_to_chs:
    push ax
    push bx

    xor dx, dx                  ; DX:AX = LBA
    mov bx, 18                  ; 18 sectors per track (standard 1.44M floppy)
    div bx                      ; AX = LBA / 18, DX = LBA % 18
    
    inc dx                      ; Sector = (LBA % 18) + 1
    mov cl, dl                  ; CL = Sector (1-based)

    xor dx, dx                  ; DX:AX = Temp
    mov bx, 2                   ; 2 heads
    div bx                      ; AX = Cylinder, DX = Head
    
    mov ch, al                  ; CH = Cylinder
    mov dh, dl                  ; DH = Head

    pop bx
    pop ax
    ret

; Enable A20 Line via BIOS, Keyboard Controller, and Port 92
enable_a20:
    ; 1. BIOS method
    mov ax, 0x2401
    int 0x15
    jnc .done

    ; 2. Keyboard Controller method
    call a20_wait
    mov al, 0xAD
    out 0x64, al                ; Disable keyboard

    call a20_wait
    mov al, 0xD0
    out 0x64, al                ; Read output port

    call a20_wait_ready
    in al, 0x60
    push ax

    call a20_wait
    mov al, 0xD1
    out 0x64, al                ; Write output port

    call a20_wait
    pop ax
    or al, 2
    out 0x60, al

    call a20_wait
    mov al, 0xAE
    out 0x64, al                ; Enable keyboard
    
    ; 3. Fast A20 method
    in al, 0x92
    or al, 2
    out 0x92, al

.done:
    ret

a20_wait:
    in al, 0x64
    test al, 2
    jnz a20_wait
    ret

a20_wait_ready:
    in al, 0x64
    test al, 1
    jz a20_wait_ready
    ret

; ------------------------------------------
; 16-Bit Real Mode Data
; ------------------------------------------

BOOT_DRIVE      db 0
STAGE2_MSG      db "Zenith OS: Stage 2 running in Real Mode.", 0x0D, 0x0A, 0
LOAD_KERNEL_MSG db "Zenith OS: Loading kernel binary from floppy...", 0x0D, 0x0A, 0
DISK_ERR_MSG    db "Zenith OS: Disk read error during kernel load!", 0x0D, 0x0A, 0
A20_MSG         db "Zenith OS: Enabling A20 gate...", 0x0D, 0x0A, 0
VESA_MSG        db "Zenith OS: Querying and setting VESA VBE 1280x1024...", 0x0D, 0x0A, 0
VESA_ERR_MSG    db "Zenith OS: Failed to set VESA VBE mode!", 0x0D, 0x0A, 0
PM_MSG          db "Zenith OS: Entering 32-bit Protected Mode...", 0x0D, 0x0A, 0

; ------------------------------------------
; Global Descriptor Table (GDT)
; ------------------------------------------

gdt_start:
    ; Null descriptor
    dd 0x0
    dd 0x0

gdt_code:
    ; Base: 0, Limit: 4GB, Type: Code, Privilege: 0 (Ring 0)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b                ; P=1, DPL=00, S=1, Type=1010 (Code, Execute/Read)
    db 11001111b                ; G=1, D=1 (32-bit), Limit(16-19)=0xF
    db 0x00

gdt_data:
    ; Base: 0, Limit: 4GB, Type: Data, Privilege: 0 (Ring 0)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b                ; P=1, DPL=00, S=1, Type=0010 (Data, Read/Write)
    db 11001111b                ; G=1, D=1 (32-bit), Limit(16-19)=0xF
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; Size
    dd gdt_start                ; Base Address

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; ==========================================
; 32-Bit Protected Mode Code
; ==========================================

[bits 32]
protected_mode_entry:
    ; Setup segment registers for Protected Mode
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Setup 32-bit stack (growing down from 0x90000)
    mov esp, 0x90000
    mov ebp, esp

    ; Print visual indicator directly to VGA Text memory at 0xB8000
    ; Prints 'P' (white on green background) at top right of screen
    mov word [0xB8000 + 158], 0x2F50

    ; Relocate the kernel from 0x10000 to 0x100000 (1MB mark)
    ; 256 sectors = 131,072 bytes = 32,768 double-words (dwords)
    mov esi, 0x10000            ; Source
    mov edi, 0x100000           ; Destination (1MB)
    mov ecx, 32768              ; Number of 32-bit dwords to copy
    rep movsd                   ; Copy memory

    ; Visual indicator: print 'K' next to 'P' to signify kernel is loaded and ready
    mov word [0xB8000 + 156], 0x2F4B

    ; Jump to kernel entry point at physical address 0x100000
    jmp CODE_SEG:0x100000

    ; Should never reach here
.halt:
    cli
    hlt
    jmp .halt
