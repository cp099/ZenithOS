# Zenith OS

```text
ZZZZZZZZ   eeee           ii    tt   hh             OOOO    SSSS  
     ZZ   ee  ee   nnnn        tttttt  hh       OO  OO   SS     
   ZZ     eeeeee   nn  nn   ii    tt    hhhhhh      OO  OO    SSSS  
  ZZ      ee       nn  nn   ii    tt    hh  hh      OO  OO       SS 
ZZZZZZZZ   eeeee   nn  nn   ii     ttt  hh  hh       OOOO    SSSS  
```

Zenith OS is a lightweight, high-resolution x86 hobby operating system designed with premium retro aesthetics, dynamic window framing, visual loading delays, single-indirect filesystem support, and customized Ring 3 userland processes.

---

## Current Architecture & System State

Zenith OS operates as a custom 32-bit x86 operating system booting from a raw MBR floppy disk image. The kernel initializes low-level CPU registers, paging, and storage drivers before loading an interactive userland shell inside Ring 3 user privilege space.

### 1. Bootloader & Video Mode Setup
* **Two-Stage Boot**: `stage1.asm` sets up the sector reads and loads the main loader `stage2.asm`.
* **VESA VBE Linear Framebuffer**: Stage 2 queries and executes BIOS VESA mode `0x411B` setting the screen dimensions to **1280x1024** pixels with 24-bit RGB True Color.
* **Boot Info block**: Key display attributes (framebuffer physical address, width, height, pitch, bpp) are written to memory address `0x7000` for the kernel to read during graphics initialization.

### 2. Kernel Core & Drivers
* **Segmentation & Privileges**: GDT and Task State Segment (TSS) are loaded to separate kernel space (Ring 0) and user space (Ring 3).
* **Interrupt Routing**: Fully remapped PIC controllers and IDT gates configure hardware interrupts for the timer and keyboard.
* **Memory Paging**: A physical and virtual memory allocator identity-maps the first 128MB of RAM using a two-level page directory, protecting kernel structures and preventing unauthorized userland memory overrides.
* **Storage & File System**: The ATA controller reads sectors off the primary hard disk where a custom **ZenithFS** is formatted. ZenithFS supports single indirect addressing allowing file maps up to 70KB in size to load separate compiled C binaries.
* **PIT System Clock**: A Programmable Interval Timer calibrated to 100Hz handles ticks and sleep states.

### 3. Graphics & Font Renderer
* **12x24 Upscaled Typography**: Text is drawn by upscaling standard 8x8 font bitmaps to 12x24 screen pixels. Bold/smooth pixel-interpolation thickens characters, giving them a high-quality solid visual look.
* **Grid Bounds & Scrolling Workspace**: Layout margins (`left/right=40`, `top=40`, `bottom=24`) enforce a perfect **100x40 console grid**. Scrolling routines copy pixel segments inside these boundaries, protecting the Slate Purple (`0x6E5F80`) desktop background and window manager title bar from console text overrides.
* **Visual Boot Delay**: Boot stages load with a paced delay showing a custom animated logo progress bar, holding the 100% finished screen for 15 seconds.

### 4. System Calls Interface (Int 0x80)
The kernel implements a custom system call vector containing 14 major wrappers exposing low-level hardware routines to userland:
* `SYS_WRITE (0)`: Prints strings to the graphic console workspace.
* `SYS_READ (1)`: Reads keyboard lines.
* `SYS_SLEEP (2)`: Suspends execution for specified clock ticks.
* `SYS_EXIT (3)`: Terminates userland execution loops.
* `SYS_SET_COLOR (4)`: Sets the current console theme colors.
* `SYS_LIST_FILES (5)`: Queries ZenithFS directory listings.
* `SYS_EXEC (6)`: Replaces process memory and executes binaries from the disk.
* `SYS_READ_FILE (7)`: Opens and reads disk files.
* `SYS_CLEAR (8)`: Clears the console workspace.
* `SYS_GETCHAR (9)`: Reads character inputs (blocking/non-blocking).
* `SYS_SET_CURSOR (10)`: Moves the cursor column/row coordinates.
* `SYS_UPTIME (11)`: Retrieves elapsed PIT timer tick count.
* `SYS_SHUTDOWN (12)`: Triggers ACPI warning state and powers off QEMU.
* `SYS_REBOOT (13)`: Triggers restart warning state and reboots the CPU.

---

## Userland Applications & Shell

Zenith OS compiles userland binaries to separate flat binaries loaded from the custom disk filesystem:

* **Interactive Shell (`sh.bin`)**: Includes custom themes (`matrix`, `retro`, `ocean`, `default`), directory traversal (`ls`), file inspection (`cat`), calculator launching (`calc`), arcade gameplay (`blaster`), system reboot (`restart`), and system power-off (`shutdown`).
* **freestanding Calculator (`calc.bin`)**: An algebraic calculator parse supporting decimal operations, nested parentheses, and operator precedence order.
* **Retro Arcade Game (`blaster.bin`)**: Spawns an arcade-style space shooter where users dodge bombs, shoot lasers at diving alien squads, and track their high scores.

---

## Screenshots

### 1. Splash Boot Screen
<img src="assets/boot_screen.png" width="500" alt="Zenith OS Boot Screen">

### 2. Interactive Terminal Shell
<img src="assets/shell_terminal.png" width="500" alt="Zenith OS Interactive Shell">

---

## Build and Run

### Prerequisites
Make sure you have `nasm`, `i686-elf-gcc`, `i686-elf-ld`, `i686-elf-objcopy`, `qemu-system-i386`, and `python3` installed.

### Commands
```bash
# Clean previous build artifacts
make clean

# Build and execute inside QEMU
make run
```
