# GitHub Copilot Instructions: Game Boy Emulator (C++)

## Project Context
* **Goal**: Develop a basic, cross-platform Game Boy (DMG) emulator in C++.
* **Primary Frameworks**: Use **SDL3** for graphics, input, and audio.
* **Build System**: Use **CMake** to ensure consistency across Windows, macOS, and Linux.
* **Philosophy**: Prioritize an understandable and modular codebase over absolute cycle-accuracy in the early stages.

## Hardware Specifications
* **CPU**: Custom Sharp SM83 (hybrid Z80/8080).
* **Clock Speed**: Approximately **4.194304 MHz**.
* **Display**: 160x144 pixel monochrome LCD with 4 shades of gray.
* **Address Space**: 16-bit address bus supporting 64 KB of memory ($0000 - $FFFF).

## Implementation Guidelines

### 1. CPU (Sharp SM83)
* **Registers**: Use `uint8_t` for individual 8-bit registers (A, F, B, C, D, E, H, L) and `uint16_t` for pairs (AF, BC, DE, HL) and special registers (SP, PC).
* **Flags (F Register)**: 
    * Bit 7: Zero Flag (Z).
    * Bit 6: Subtract Flag (N).
    * Bit 5: Half Carry Flag (H).
    * Bit 4: Carry Flag (C).
    * **Note**: Bits 3-0 must always remain zero.
* **Instruction Set**: Implement approximately 256 base opcodes and 256 prefixed `0xCB` opcodes using a large switch statement or function pointer table.
* **Timing**: Accurate timing is essential for synchronization; 1 M-cycle equals 4 T-states.

### 2. Memory Map (MMU)
Route memory access based on these regions:
* `$0000 - $3FFF`: 16 KB ROM Bank 00 (fixed).
* `$4000 - $7FFF`: 16 KB Switchable ROM Bank.
* `$8000 - $9FFF`: 8 KB Video RAM (VRAM).
* `$A000 - $BFFF`: 8 KB External RAM (Cartridge RAM).
* `$C000 - $DFFF`: 8 KB Work RAM (WRAM).
* `$E000 - $FDFF`: Echo RAM (Mirror of `$C000 - $DDFF`).
* `$FE00 - $FE9F`: Object Attribute Memory (OAM).
* `$FF00 - $FF7F`: I/O Registers.
* `$FF80 - $FFFE`: High RAM (HRAM).
* `$FFFF`: Interrupt Enable (IE) Register.

### 3. Graphics (PPU)
* **Rendering Cycle**: 154 scanlines total (144 visible, 10 VBlank).
* **Timing**: Each scanline takes 456 T-states; one full frame takes 70224 T-states.
* **Modes**: Implement transitions between OAM Scan (Mode 2), Drawing (Mode 3), HBlank (Mode 0), and VBlank (Mode 1).
* **Sprites**: Support up to 40 sprites total and a maximum of 10 sprites per scanline.

### 4. Memory Bank Controllers (MBC)
* **Cartridge Header**: Detect MBC type by reading address `$0147`.
* **MBC1**: Handle bank switching for ROM (up to 2 MB) and RAM (up to 32 KB) via writes to ROM address space.

## Coding Standards
* **Types**: Use `<cstdint>` fixed-width types (e.g., `uint8_t`, `uint16_t`) to ensure cross-platform size consistency.
* **Endianness**: The Game Boy is little-endian.
* **Portability**: Avoid platform-dependent APIs; rely on SDL3 for windowing and hardware abstraction.

## Testing & Debugging
* **Skipping Boot ROM**: For initial development, set the Program Counter (PC) to `$0100` and initialize registers to post-boot values.
* **Verification**: Use **Blargg's Test ROMs** (specifically `cpu_instrs.gb`) to verify instruction logic.
* **Logging**: Implement CPU state logging (PC, registers, flags) for comparison against reference emulators like BGB.