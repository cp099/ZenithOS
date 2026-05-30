# ZenithOS

```text
 ZZZZZZ  EEEEEE  NN  NN  IIIIII  TTTTTT  HH  HH    OOOO    SSSS 
     ZZ  EE      NNN NN    II      TT    HH  HH   OO  OO  SS    
   ZZZ   EEEE    ######    II      TT    ######   OO  OO   SSSS 
  ZZ     EE      NN ###    II      TT    HH  HH   OO  OO      SS
 ZZZZZZ  EEEEEE  NN  NN  IIIIII    TT    HH  HH    OOOO    SSSS 
```

ZenithOS is a lightweight, high-resolution x86 hobby operating system designed with premium retro aesthetics, dynamic window framing, visual loading delays, single-indirect filesystem support, and customized Ring 3 userland processes.

## Key Features

* **High-Resolution VESA Graphics**: Runs natively in **1280x1024** VESA VBE mode with a 24-bit linear framebuffer.
* **Slick Bold Font Engine**: Implements dynamic upscale character rendering at **12x24** pixel bounds with bold pixel-interpolation to keep text clean, sharp, and readable.
* **Visual Splash Boot Screen**: Centered dynamic layout featuring an animated progress bar and boot phase status messages with a custom 15-second visual pacing delay.
* **ACPI Power Management**: Handles kernel-level shutdown routines (via QEMU ACPI ports) and hardware restart loops (via Keyboard Controller port `0x64`), complete with styled power-state graphic screens.
* **ZenithFS Storage**: Custom file system utilizing single indirect addressing to support executable binaries up to 70KB in size.
* **Ring 3 Userland & Libc**: Custom syscall dispatcher (`int 0x80`) backing a standard I/O library and interactive freestanding applications:
  * `sh.bin`: The interactive shell with customizable themes (default, matrix, ocean, retro).
  * `calc.bin`: A mathematical calculator supporting nested parenthesis and operator precedence.
  * `blaster.bin`: A margined retro arcade space invaders game with laser shooting, alien diving, and score/live trackers.

## Screenshots

### 1. Splash Boot Screen
<img src="assets/boot_screen.png" width="500" alt="ZenithOS Boot Screen">

### 2. Interactive Terminal Shell
<img src="assets/shell_terminal.png" width="500" alt="ZenithOS Interactive Shell">

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
