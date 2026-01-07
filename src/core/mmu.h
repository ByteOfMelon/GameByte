#pragma once
#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <iomanip>

class CPU;
class PPU;

/**
 * @brief Implements the Game Boy's Memory Management Unit (MMU).alignas
 * 
 * The GB has a 64 KB address space ($0000-$FFFF).
 * 
 * Memory Map - The Game Boy has a "memory map" of sorts that allocates different parts of memory to certain functions, such as:
 * 
 * Main Cartridge ROM - First 16 KB of address space ($0000-$3FFF) which loads a cartridge ROM. 
 * Bankable Cartridge ROM - Next 16 KB of address space ($4000-$7FFF). A memory bank controller (MBC) can be used to swap different banks into this addressable space.
 * VRAM - The next 8 KB of address space ($8000-$9FFF). Stores graphical data.
 * "External RAM" - 8 KB of address space ($A000-$BFFF), which can also have banks via an MBC. Used for battery backup saves stored on-cart.
 * WRAM (Work RAM) - 8 KB of address space ($C000-$DFFF). Similar to external RAM but is not battery-backed up.
 * Echo RAM - 8KB of address space ($E000-$FDFF) Mirrors WRAM. Reads/writes to this region MUST be redirected to their corresponding WRAM addresses. Nintendo does not allow use of this area.
 * (Note that memory locations $FEA0-$FEFF) are "not usable" as per Nintendo's specs.
 * OAM - Object Attribute Memory ($FE00-$FE9F). A 160-byte array for sprite attributes.
 * I/O Registers - ($FF00-$FF7F) - Each register needs custom read/write handling logic, as they control hardware behavior.
 * High RAM ($FF80-$FFFE) - A small byte array.
 * Interupt Enable Register ($FFFF) -  A single byte.
 */
class MMU {
    public:
        MMU();

        CPU* cpu = nullptr;
        void connect_cpu(CPU* c);

        PPU* ppu = nullptr;
        void connect_ppu(PPU* p);

        uint8_t read_byte(uint16_t address);
        void write_byte(uint16_t address, uint8_t value);

        uint16_t read_word(uint16_t address);
        void write_word(uint16_t address, uint16_t value);
        
        bool load_game(const uint8_t* data, size_t size);

        // Debug functions to dump HRAM/VRAM contents
        void dump_hram();
        void dump_vram();
    private:
        unsigned char cart[0x8000]; // 32 KB total cartridge ROM space
        unsigned char vram[0x2000]; // 8 KB of video RAM (VRAM)
        unsigned char eram[0x2000]; // 8 KB of external RAM (cartridge battery-backed RAM)
        unsigned char wram[0x2000]; // 8 KB of work RAM (WRAM). In CGB mode, this is switchable banks 1-7
        unsigned char oam[0xA0];    // 160 bytes for sprite attribute memory (OAM)
        unsigned char io[0x80];     // 128 bytes for I/O registers
        unsigned char hram[0x7F];   // 127 bytes for high RAM
        uint8_t ie;                 // Interrupt Enable register (IE) at 0xFFFF
};
