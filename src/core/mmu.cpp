#include "mmu.h"
#include "cpu.h"
#include "ppu.h"
#include "joypad.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

MMU::MMU() {
    // Initialize memory arrays and variables
    memset(cart, 0, sizeof(cart));
    memset(vram, 0, sizeof(vram));
    memset(eram, 0, sizeof(eram));
    memset(wram, 0, sizeof(wram));
    memset(oam, 0, sizeof(oam));
    memset(io, 0, sizeof(io));
    memset(hram, 0, sizeof(hram));
    ie = 0;
}

void MMU::connect_cpu(CPU* c) {
    cpu = c;
}

void MMU::connect_ppu(PPU* p) {
    ppu = p;
}

void MMU::connect_joypad(Joypad* j) {
    joypad = j;
}

bool MMU::load_game(const uint8_t* data, size_t size) {
    // Clear cartridge memory
    memset(cart, 0, sizeof(cart));

    if (size > sizeof(cart)) {
        // For now, if ROM is larger than 32KB, throw an error. In the future this needs to be handled by MBC logic.
        throw std::runtime_error("[MMU] ERROR: ROM size (" + std::to_string(size) + ") larger than 32KB. Bank switching/MBC logic is not currently supported.");
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
    if (address <= 0x7FFF) {
        // Cartridge ROM
        return cart[address];
    } else if (address <= 0x9FFF) {
        // VRAM
        return vram[address - 0x8000];
    } else if (address <= 0xBFFF) {
        // External RAM
        return eram[address - 0xA000];
    } else if (address <= 0xDFFF) {
        // Work RAM
        return wram[address - 0xC000];
    } else if (address <= 0xFDFF) {
        // Echo RAM (mirror of Work RAM)
        return wram[address - 0xE000];
    } else if (address <= 0xFE9F) {
        // Object Attribute Memory (OAM)
        return oam[address - 0xFE00];
    } else if (address >= 0xFF00 && address <= 0xFF7F) {
        // Joypad (0xFF00)
        if (address == 0xFF00) {
            return joypad ? joypad->get_joyp_state() : 0xFF;
        }   

        // I/O Registers
        if (address == 0xFF04 && cpu) {
            std::cout << "[MMU] DEBUG: Reading DIV register at 0xFF04, returning upper 8 bits of internal counter: " << std::hex << (cpu->internal_counter >> 8) << std::dec << std::endl;
            return static_cast<uint8_t>(cpu->internal_counter >> 8);
        }
        
        // PPU Registers read delegation
        if (address >= 0xFF40 && address <= 0xFF47 && ppu) {
            switch (address) {
                case 0xFF40: return ppu->get_lcdc();
                case 0xFF41: return ppu->get_stat();
                case 0xFF42: return ppu->get_scy();
                case 0xFF43: return ppu->get_scx();
                case 0xFF44: return ppu->get_ly();
                case 0xFF45: return ppu->get_lyc();
                case 0xFF47: return ppu->get_bgp();
            }
        }
        
        return io[address - 0xFF00];
    } else if (address >= 0xFF80 && address <= 0xFFFE) {
        // High RAM
        return hram[address - 0xFF80];
    } else if (address == 0xFFFF) {
        // Interupt Enable Register
        return ie;
    } else {
        // Unusable memory area or not implemented (e.g. 0xFEA0 - 0xFEFF)
        ss << "Attempted read of unusable memory area at 0x" << std::hex << address;
        throw std::runtime_error("[MMU] " + ss.str());
    }
}

void MMU::write_byte(uint16_t address, uint8_t value) {
    // Stream for errors
    std::stringstream ss;

    // Special write cases (i.e. I/O registers, VRAM, etc)
    // Joypad
    if (address == 0xFF00) {
        if (joypad) {
            // Only bits 4 and 5 are writable by the CPU
            joypad->control_mask = (value & 0x30);
        }
        return;
    }

    // PPU
    if (address >= 0xFF40 && address <= 0xFF47) {
        // Always update the I/O memory map so reads (like in PPU::draw_scanline) get the correct value
        io[address - 0xFF00] = value;

        if (!ppu) return;
        switch (address) {
            case 0xFF40: ppu->set_lcdc(value); break;
            case 0xFF41: ppu->set_stat(value); break;
            case 0xFF42: ppu->set_scy(value);  break;
            case 0xFF43: ppu->set_scx(value);  break;
            case 0xFF44: ppu->reset_ly();      break;
            case 0xFF45: ppu->set_lyc(value);  break;

            // DMA Transfer (0xFF46)
            case 0xFF46:
                // Value written is the high byte of source address
                for (int i = 0; i < 160; i++) {
                    write_byte(0xFE00+i, read_byte((value << 8)+i));
                } 
                break;
                
            case 0xFF47: ppu->set_bgp(value);  break;
        }
        return;
    }

    // DIV register (0xFF04)
    if (address == 0xFF04) {
        // Writing to DIV register resets it
        cpu->reset_internal_counter();
        return;
    }

    // TIMA (0xFF05), TMA (0xFF06), TAC (0xFF07)
    if (address >= 0xFF05 && address <= 0xFF07) {
        io[address - 0xFF00] = value;
        return;
    }

    // Other bytes - find byte in memory map
    if (address <= 0x7FFF) {
        // Cartridge ROM is read-only directly, but used for MBC commands
        // TODO: Implement MBC banking support
    } else if (address <= 0x9FFF) {
        // VRAM
        vram[address - 0x8000] = value;
    } else if (address <= 0xBFFF) {
        // External RAM
        eram[address - 0xA000] = value;
    } else if (address <= 0xDFFF) {
        // Work RAM
        wram[address - 0xC000] = value;
    } else if (address <= 0xFDFF) {
        // Echo RAM (mirror of Work RAM)
        wram[address - 0xE000] = value;
    } else if (address <= 0xFE9F) {
        // Object Attribute Memory (OAM)
        oam[address - 0xFE00] = value;
    } else if (address <= 0xFEFF) {
        // Unusable memory (0xFEA0 ... 0xFEFF)
        // Writes to this area are ignored
    } else if (address <= 0xFF7F) {
        // I/O Registers (general/unimplemented)
        io[address - 0xFF00] = value;
    } else if (address <= 0xFFFE) {
        // High RAM
        hram[address - 0xFF80] = value;
    } else if (address == 0xFFFF) {
        // Interupt Enable Register
        ie = value;
    } else {
        // Unusable memory area or not implemented
        ss << "Attempted write to unusable memory area at 0x" << std::hex << address;
        throw std::runtime_error("[MMU] " + ss.str());
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

void MMU::dump_hram() {
    std::cout << "--- HRAM DUMP ($FF80 - $FFFE) ---" << std::endl;
    for (uint16_t i = 0xFF80; i <= 0xFFFE; i++) {
        if ((i - 0xFF80) % 16 == 0) std::cout << std::endl << std::hex << i << ": ";
        std::cout << (int)read_byte(i) << " ";
    }
    std::cout << std::dec << std::endl << "--------------------------------" << std::endl;
}

void MMU::dump_vram() {
    uint8_t lcdc = 0;
    if (ppu) lcdc = ppu->get_lcdc();
    
    std::cout << "--- PPU REGISTERS ---" << std::endl;
    std::cout << "LCDC: 0x" << std::hex << (int)lcdc << std::dec;
    std::cout << " (BG:" << ((lcdc & 0x01) ? "ON" : "OFF");
    std::cout << " Tiles:" << ((lcdc & 0x10) ? "8000" : "8800");
    std::cout << " Map:" << ((lcdc & 0x08) ? "9C00" : "9800") << ")" << std::endl;

    std::cout << "--- VRAM TILE DATA (First 16 bytes of 0x8000) ---" << std::endl;
    for (int i = 0; i < 16; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)vram[i] << " ";
    }
    std::cout << std::dec << std::endl;

    std::cout << "--- BG MAP 0x9800 (First 32 bytes) ---" << std::endl;
    for (int i = 0; i < 32; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)vram[0x1800 + i] << " ";
    }
    std::cout << std::dec << std::endl;

    std::cout << "--- BG MAP 0x9C00 (First 32 bytes) ---" << std::endl;
    for (int i = 0; i < 32; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)vram[0x1C00 + i] << " ";
    }
    std::cout << std::dec << std::endl;

    // Check for any data in Maps
    int map1_count = 0;
    for (int i = 0x1800; i < 0x1C00; i++) if (vram[i] != 0) map1_count++;
    
    int map2_count = 0;
    for (int i = 0x1C00; i < 0x2000; i++) if (vram[i] != 0) map2_count++;

    std::cout << "Non-zero bytes in 9800 Map: " << map1_count << std::endl;
    std::cout << "Non-zero bytes in 9C00 Map: " << map2_count << std::endl;
    std::cout << "--------------------------------" << std::endl;
}