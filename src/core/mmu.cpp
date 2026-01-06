#include "mmu.h"
#include <cstring>

bool MMU::load_game(const uint8_t* data, size_t size) {
    // Clear cartridge memory
    memset(cart, 0, sizeof(cart));

    if (size > sizeof(cart)) {
        // For now, if ROM is larger than 32KB, we just load the first 32KB.
        // In the future this needs to be handled by MBC logic.
        std::cerr << "[MMU] Warning: ROM size (" << size << ") larger than 32KB. Bank switching is not currently supported, loading first 32 KB only." << std::endl;
        std::memcpy(cart, data, sizeof(cart));
    } else {
        std::memcpy(cart, data, size);
    }
    
    return true;
}

uint8_t MMU::read_byte(uint16_t address) {
    // Stream for errors
    std::stringstream ss;

    // Find byte in memory map
    switch (address) {
        // Cartridge ROM
        case 0x0000 ... 0x7FFF:
            return cart[address];

        // VRAM
        case 0x8000 ... 0x9FFF:
            return vram[address - 0x8000];

        // External RAM
        case 0xA000 ... 0xBFFF:
            return eram[address - 0xA000];

        // Work RAM
        case 0xC000 ... 0xDFFF:
            return wram[address - 0xC000];

        // Echo RAM (mirror of Work RAM)
        case 0xE000 ... 0xFDFF:
            return wram[address - 0xE000];

        // Object Attribute Memory (OAM)
        case 0xFE00 ... 0xFE9F:
            return oam[address - 0xFE00];

        // I/O Registers
        case 0xFF00 ... 0xFF7F:
            return io[address - 0xFF00];

        // High RAM
        case 0xFF80 ... 0xFFFE:
            return hram[address - 0xFF80];

        // Interupt Enable Register
        case 0xFFFF:
            return ie;

        // Unusable memory area or not implemented
        default:
            ss << "Attempted read of unusable memory area at 0x" << std::hex << address;
            throw std::runtime_error("[MMU] " + ss.str());
            break;
    }
}

void MMU::write_byte(uint16_t address, uint8_t value) {
    // Stream for errors
    std::stringstream ss;

    // Find byte in memory map
    switch (address) {
        // Cartridge ROM is read-only and therefore is invalid
        case 0x0000 ... 0x7FFF:
            ss << "Attempted write to ROM";
            throw std::runtime_error("[MMU] " + ss.str());
            break;
        // VRAM
        case 0x8000 ... 0x9FFF:
            vram[address - 0x8000] = value;
            break;      
        // External RAM
        case 0xA000 ... 0xBFFF:
            eram[address - 0xA000] = value;
            break;
        // Work RAM
        case 0xC000 ... 0xDFFF:
            wram[address - 0xC000] = value;
            break;
        // Echo RAM (mirror of Work RAM)
        case 0xE000 ... 0xFDFF:
            wram[address - 0xE000] = value;
            break;
        // Object Attribute Memory (OAM)
        case 0xFE00 ... 0xFE9F:
            oam[address - 0xFE00] = value;
            break;
        // I/O Registers
        case 0xFF00 ... 0xFF7F:
            io[address - 0xFF00] = value;
            break;
        // High RAM
        case 0xFF80 ... 0xFFFE:
            hram[address - 0xFF80] = value;
            break;
        // Interupt Enable Register
        case 0xFFFF:
            ie = value;
            break;
        // Unusable memory area or not implemented
        default:
            ss << "Attempted write to unusable memory area at 0x" << std::hex << address;
            throw std::runtime_error("[MMU] " + ss.str());
            break;
    }
}

uint16_t MMU::read_word(uint16_t address) {
    // Read strictly little-endian
    return read_byte(address) | (read_byte(address + 1) << 8);
}

void MMU::write_word(uint16_t address, uint16_t value) {
    // Write strictly little-endian
    write_byte(address, value & 0xFF);
    write_byte(address + 1, (value >> 8) & 0xFF);
}
