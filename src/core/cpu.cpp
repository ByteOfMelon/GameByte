#include "cpu.h"
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <iomanip>

CPU::CPU() {
    init_instructions();

    // Initialize registers (simple power-on state, usually PC=0x0100 for post-bootROM)
    pc = 0x0100;
    sp = 0xFFFE;
    a = 0x01; f = 0xB0;
    b = 0x00; c = 0x13;
    d = 0x00; e = 0xD8;
    h = 0x01; l = 0x4D;
    internal_counter = 0;
    
    ime = false;
}

uint16_t CPU::get_af() const { 
    return (static_cast<uint16_t>(a) << 8) | f;
}

void CPU::set_af(uint16_t value) {
    a = (value >> 8) & 0xFF; 
    f = value & 0xF0;
}

uint16_t CPU::get_bc() const { 
    return (static_cast<uint16_t>(b) << 8) | c;
}

void CPU::set_bc(uint16_t value) {
    b = (value >> 8) & 0xFF;
    c = value & 0xFF;
}

uint16_t CPU::get_de() const { 
    return (static_cast<uint16_t>(d) << 8) | e;
}

void CPU::set_de(uint16_t value) {
    d = (value >> 8) & 0xFF;
    e = value & 0xFF;
}

uint16_t CPU::get_hl() const { 
    return (static_cast<uint16_t>(h) << 8) | l;
}

void CPU::set_hl(uint16_t value) {
    h = (value >> 8) & 0xFF;
    l = value & 0xFF;
}

bool CPU::get_flag_z() const { 
    return (f & 0x80) != 0; 
}

void CPU::set_flag_z(bool value) { 
    f = (value)? (f | 0x80) : (f & ~0x80);
}

bool CPU::get_flag_n() const { 
    return (f & 0x40) != 0; 
}

void CPU::set_flag_n(bool value) { 
    f = (value)? (f | 0x40) : (f & ~0x40);
}

bool CPU::get_flag_h() const { 
    return (f & 0x20) != 0; 
}

void CPU::set_flag_h(bool value) { 
    f = (value)? (f | 0x20) : (f & ~0x20);
}

bool CPU::get_flag_c() const { 
    return (f & 0x10) != 0; 
}

void CPU::set_flag_c(bool value) { 
    f = (value)? (f | 0x10) : (f & ~0x10);
}

void CPU::connect_mmu(MMU* m) {
    mmu = m;

    // Initialize MMU state
    mmu->write_byte(0xFF40, 0x91); // LCDC: LCD On, BG On, Window Off, etc.

    // Set a standard palette so colors aren't all white/transparent
    mmu->write_byte(0xFF47, 0xFC); // BGP: Standard gray shades (11 11 11 00)

    // Clear the interrupt registers
    mmu->write_byte(0xFF0F, 0x00); // IF
    mmu->write_byte(0xFFFF, 0x00); // IE
}

void CPU::tick_timers(uint8_t cycles) {
    // Save old counter to check for falling edges
    uint16_t old_counter = internal_counter;
    internal_counter += cycles;

    // Get TAC register (timer control)
    uint8_t tac = mmu->read_byte(0xFF07);

    // If timer is enabled
    if (tac & 0x04) {
        // Determine the bit we are watching for a falling edge on

        // Reference:
        // 00: 4096 Hz   = Bit 9
        // 01: 262144 Hz = Bit 3
        // 10: 65536 Hz  = Bit 5
        // 11: 16384 Hz  = Bit 7
        uint16_t bit_mask = 0;
        switch (tac & 0x03) {
            case 0x00: bit_mask = (1 << 9); break;
            case 0x01: bit_mask = (1 << 3); break;
            case 0x02: bit_mask = (1 << 5); break;
            case 0x03: bit_mask = (1 << 7); break;
        }

        // Check for falling edge: was high, now low
        bool old_bit = (old_counter & bit_mask);
        bool new_bit = (internal_counter & bit_mask);

        if (old_bit && !new_bit) {
            // Increment TIMA
            uint8_t tima = mmu->read_byte(0xFF05);
            tima++;
            
            // Check for overflow (0xFF -> 0x00)
            if (tima == 0x00) {
                // Determine value to reload from TMA
                uint8_t tma = mmu->read_byte(0xFF06);
                tima = tma;

                // Request timer interrupt (Bit 2 of IF)
                uint8_t if_reg = mmu->read_byte(0xFF0F);
                mmu->write_byte(0xFF0F, if_reg | 0x04);
            }

            mmu->write_byte(0xFF05, tima);
        }
    }
} 

void CPU::reset_internal_counter() {
    internal_counter = 0;
}

uint8_t CPU::handle_interrupts() {
    uint8_t if_reg = mmu->read_byte(0xFF0F);
    uint8_t ie_reg = mmu->read_byte(0xFFFF);
    uint8_t pending = if_reg & ie_reg;

    // Any pending interrupt wakes the CPU
    if (pending > 0) {
        this->halted = false;
    }

    if (ime && pending > 0) {
        // Services the highest priority interrupt first

        // Priority 1: V-Blank (0x0040)
        if (pending & 0x01) {
            printf("[INTERRUPT] Dispatching to 0x0040!\n");
            return execute_interrupt(0, 0x0040);
        }

        // Priority 2: LCD STAT (0x0048)
        if (pending & 0x02) {
            printf("[INTERRUPT] Dispatching to 0x0048!\n");
            return execute_interrupt(1, 0x0048);
        }

        // Priority 3: Timer (0x0050)
        if (pending & 0x04) {
            printf("[INTERRUPT] Dispatching to 0x0050!\n");
            return execute_interrupt(2, 0x0050);
        }

        // Priority 4: Serial (0x0058)
        if (pending & 0x08) {
            printf("[INTERRUPT] Dispatching to 0x0058!\n");
            return execute_interrupt(3, 0x0058);
        }

        // Priority 5: Joypad (0x0060)
        if (pending & 0x10) {
            printf("[INTERRUPT] Dispatching to 0x0060!\n");
            return execute_interrupt(4, 0x0060);
        }
    }

    // If no interupts needed servicing, return 0 cycles
    return 0;
}

uint8_t CPU::execute_interrupt(uint8_t bit, uint16_t vector) {
    ime = false;
    
    // Clear the specific interrupt bit in IF register
    uint8_t if_reg = mmu->read_byte(0xFF0F);
    mmu->write_byte(0xFF0F, if_reg & ~(1 << bit));

    // Push PC to stack
    sp -= 2;
    mmu->write_word(sp, pc);

    // Jump to the interrupt handler vector
    pc = vector;

    // Interrupt dispatch takes approximately 20 clock cycles total
    // (5 machine cycles: 2 for internal logic + 2 for pushing PC + 1 for jump)
    return 20; 
}

uint8_t CPU::step() {
    if (!mmu) {
        throw std::runtime_error("[CPU] MMU was not connected to CPU before execution");
    }

    // Interupt handling
    uint8_t int_cycles = handle_interrupts();
    if (int_cycles > 0) {
        total_cycles += int_cycles;
        return int_cycles; 
    }

    // If halted, skip instruction execution
    if (halted) {
        total_cycles += 4;
        return 4;
    }

    uint8_t opcode = mmu->read_byte(pc);

    // Enabling the below debug line will produce a LOT of output and cause significant slowdown
    // printf("[CPU] DEBUG: Executing opcode 0x%02X (instruction %s) at address 0x%04X\n", opcode, instructions[opcode].name, pc);
    pc++;

    uint8_t cycles = (this->*instructions[opcode].operate)();

    // Handle IME delay
    if (this->ime_delay > 0) {
        this->ime_delay--;
        if (this->ime_delay == 0) {
            this->ime = true;
        }
    }

    total_cycles += cycles;
    return cycles;
}

void CPU::debug_interrupt_status() {
    uint8_t if_reg = mmu->read_byte(0xFF0F); // Interrupt flag (requests)
    uint8_t ie_reg = mmu->read_byte(0xFFFF); // Interrupt enable
    uint8_t ly_reg = mmu->read_byte(0xFF44); // Current scanline
    uint8_t lcdc   = mmu->read_byte(0xFF40); // LCD control

    std::stringstream ss;
    ss << "--- PPU/INT STATUS ---" << std::endl;
    ss << "LY (Scanline): " << (int)ly_reg << std::endl;
    ss << "LCD Enabled:   " << ((lcdc & 0x80) ? "YES" : "NO") << std::endl;
    ss << "IME (Master):  " << (ime ? "ON" : "OFF") << std::endl;
    ss << "IE (Enabled):  0x" << std::hex << (int)ie_reg << std::dec << std::endl;
    ss << "IF (Pending):  0x" << std::hex << (int)if_reg << std::dec << std::endl;
    ss << "----------------------" << std::endl;

    std::cout << ss.str();
}

// Extended opcode implementation 
uint8_t CPU::execute_cb_instruction(uint8_t opcode) {
    // Determine register target based on bottom 3 bits
    uint8_t* registers[] = { &b, &c, &d, &e, &h, &l, nullptr, &a };
    uint8_t target_idx = opcode & 0x07;
    
    // Most CB instructions take 8 cycles, but [HL] operations take 16
    uint8_t cycles = (target_idx == 6) ? 16 : 8;

    uint8_t value;
    // Check if target is memory ([HL]) or register
    if (target_idx == 6) {
        value = mmu->read_byte(get_hl());
    } else {
        value = *registers[target_idx];
    }

    // Decode the top two bits for the category
    switch (opcode >> 6) {
        // Shifts and Rotates (0x00 - 0x3F)
        case 0x00:
            value = handle_cb_shift_rotate(opcode, value);
            set_flag_z(value == 0);
            set_flag_n(false);
            set_flag_h(false);
            break;

        // BIT (0x40 - 0x7F)
        case 0x01:
            {
                uint8_t bit = (opcode >> 3) & 0x07;
                set_flag_z(!(value & (1 << bit)));
                set_flag_n(false);
                set_flag_h(true);

                // BIT doesn't write back
                return cycles;
            }

        // RES (0x80 - 0xBF)
        case 0x02:
            {
                uint8_t bit = (opcode >> 3) & 0x07;
                value &= ~(1 << bit);
            }
            break;

        // SET (0xC0 - 0xFF)
        case 0x03:
            {
                uint8_t bit = (opcode >> 3) & 0x07;
                value |= (1 << bit);
            }
            break;

        // BIT 7, H
        case 0x7C:
            set_flag_z(!(h & 0x80));
            set_flag_n(false);
            set_flag_h(true);
            break;
    }

    // Write the result back
    if (target_idx == 6) {
        mmu->write_byte(get_hl(), value);
    } else {
        *registers[target_idx] = value;
    }

    return cycles;
}

uint8_t CPU::handle_cb_shift_rotate(uint8_t opcode, uint8_t value) {
    uint8_t sub_op = (opcode >> 3) & 0x07;
    bool old_carry = get_flag_c();

    switch (sub_op) {
        // RLC (Rotate Left)
        case 0:
            set_flag_c(value & 0x80);
            value = (value << 1) | (value >> 7);
            break;

        // RRC (Rotate Right)
        case 1:
            set_flag_c(value & 0x01);
            value = (value >> 1) | (value << 7);
            break;
        
        // RL (Rotate Left through Carry)
        case 2:
            set_flag_c(value & 0x80);
            value = (value << 1) | (old_carry ? 1 : 0);
            break;

        // RR (Rotate Right through Carry)
        case 3:
            set_flag_c(value & 0x01);
            value = (value >> 1) | (old_carry ? 0x80 : 0);
            break;

        // SLA (Shift Left Arithmetic)
        case 4:
            set_flag_c(value & 0x80);
            value <<= 1;
            break;

        // SRA (Shift Right Arithmetic - preserve bit 7)
        case 5:
            set_flag_c(value & 0x01);
            value = (static_cast<int8_t>(value)) >> 1;
            break;
        
        // SWAP (Swap nibbles)
        case 6:
            set_flag_c(false);
            value = ((value & 0x0F) << 4) | ((value & 0xF0) >> 4);
            break;

        // SRL (Shift Right Logical)
        case 7:
            set_flag_c(value & 0x01);
            value >>= 1;
            break;
    }

    set_flag_z(value == 0);
    set_flag_n(false);
    set_flag_h(false);
    return value;
}

void CPU::init_instructions() {
    instructions.assign(256, { "XXX", &CPU::XXX });
    instructions[0x00] = { "NOP", &CPU::NOP };

    instructions[0xC3] = { "JP a16", &CPU::JP_a16 };
    instructions[0xC2] = { "JP NZ, a16", &CPU::JP_NZ_a16 };
    instructions[0xCA] = { "JP Z, a16", &CPU::JP_Z_a16 };
    instructions[0xD2] = { "JP NC, a16", &CPU::JP_NC_a16 };
    instructions[0xDA] = { "JP C, a16", &CPU::JP_C_a16 };
    
    instructions[0xAF] = { "XOR A, A", &CPU::XOR_A_A };
    instructions[0xA8] = { "XOR A, B", &CPU::XOR_A_B };
    instructions[0xA9] = { "XOR A, C", &CPU::XOR_A_C };
    instructions[0xAA] = { "XOR A, D", &CPU::XOR_A_D };
    instructions[0xAB] = { "XOR A, E", &CPU::XOR_A_E };
    instructions[0xEE] = { "XOR A, n8", &CPU::XOR_A_n8 };

    instructions[0x06] = { "LD B, n8", &CPU::LD_B_n8 };
    instructions[0x08] = { "LD [a16], SP", &CPU::LD_a16_SP };
    instructions[0x0E] = { "LD C, n8", &CPU::LD_C_n8 };
    instructions[0x16] = { "LD D, n8", &CPU::LD_D_n8 };
    instructions[0x1E] = { "LD E, n8", &CPU::LD_E_n8 };
    instructions[0x26] = { "LD H, n8", &CPU::LD_H_n8 };
    instructions[0x2E] = { "LD L, n8", &CPU::LD_L_n8 };
    instructions[0x36] = { "LD [HL], n8", &CPU::LD_HL_n8 };
    instructions[0x3E] = { "LD A, n8", &CPU::LD_A_n8 };

    instructions[0x22] = { "LD (HL+), A", &CPU::LD_HL_ptr_inc_A };
    instructions[0x32] = { "LD (HL-), A", &CPU::LD_HL_ptr_dec_A };

    instructions[0x3D] = { "DEC A", &CPU::DEC_A };
    instructions[0x05] = { "DEC B", &CPU::DEC_B };
    instructions[0x0D] = { "DEC C", &CPU::DEC_C };
    instructions[0x15] = { "DEC D", &CPU::DEC_D };
    instructions[0x1D] = { "DEC E", &CPU::DEC_E };
    instructions[0x25] = { "DEC H", &CPU::DEC_H };
    instructions[0x2D] = { "DEC L", &CPU::DEC_L };
    instructions[0x35] = { "DEC [HL]", &CPU::DEC_at_HL };
    instructions[0x20] = { "JR NZ, e8", &CPU::JR_NZ_e8 };
    instructions[0x18] = { "JR e8", &CPU::JR_e8 };
    instructions[0xF3] = { "DI", &CPU::DI };
    instructions[0xFB] = { "EI", &CPU::EI };
    instructions[0xE0] = { "LDH [a8], A", &CPU::LDH_a8_a };
    instructions[0xF0] = { "LDH A, [a8]", &CPU::LDH_a_a8 };
    
    instructions[0xFE] = { "CP A, n8", &CPU::CP_A_n8 };
    instructions[0xBF] = { "CP A, A", &CPU::CP_A_A };
    instructions[0xB8] = { "CP A, B", &CPU::CP_A_B };
    instructions[0xB9] = { "CP A, C", &CPU::CP_A_C };
    instructions[0xBA] = { "CP A, D", &CPU::CP_A_D };
    instructions[0xBB] = { "CP A, E", &CPU::CP_A_E };
    instructions[0xBC] = { "CP A, H", &CPU::CP_A_H };
    instructions[0xBD] = { "CP A, L", &CPU::CP_A_L };
    instructions[0xBE] = { "CP A, [HL]", &CPU::CP_at_HL };

    instructions[0xCD] = { "CALL a16", &CPU::CALL_a16 };
    instructions[0xC9] = { "RET", &CPU::RET };
    instructions[0xD9] = { "RETI", &CPU::RETI };
    instructions[0x76] = { "HALT", &CPU::HALT };
    instructions[0x77] = { "LD (HL), A", &CPU::LD_HL_ptr_A };
    instructions[0xEA] = { "LD [a16], A", &CPU::LD_a16_A };
    instructions[0x2A] = { "LD A, (HL+)", &CPU::LD_A_HL_ptr_inc };
    instructions[0x3A] = { "LD A, (HL-)", &CPU::LD_A_HL_ptr_dec };

    instructions[0x09] = { "ADD HL, BC", &CPU::ADD_HL_BC };
    instructions[0x19] = { "ADD HL, DE", &CPU::ADD_HL_DE };
    instructions[0x29] = { "ADD HL, HL", &CPU::ADD_HL_HL };
    instructions[0x39] = { "ADD HL, SP", &CPU::ADD_HL_SP };

    instructions[0x3C] = { "INC A", &CPU::INC_A };
    instructions[0x04] = { "INC B", &CPU::INC_B };
    instructions[0x0C] = { "INC C", &CPU::INC_C };
    instructions[0x14] = { "INC D", &CPU::INC_D };
    instructions[0x1C] = { "INC E", &CPU::INC_E };
    instructions[0x24] = { "INC H", &CPU::INC_H };
    instructions[0x2C] = { "INC L", &CPU::INC_L };
    instructions[0x34] = { "INC [HL]", &CPU::INC_at_HL };

    instructions[0x03] = { "INC BC", &CPU::INC_BC };
    instructions[0x13] = { "INC DE", &CPU::INC_DE };
    instructions[0x23] = { "INC HL", &CPU::INC_HL };
    instructions[0x33] = { "INC SP", &CPU::INC_SP };

    instructions[0x01] = { "LD BC, n16", &CPU::LD_BC_n16 };
    instructions[0x02] = { "LD (BC), A", &CPU::LD_BC_ptr_A };
    instructions[0x11] = { "LD DE, n16", &CPU::LD_DE_n16 };
    instructions[0x12] = { "LD (DE), A", &CPU::LD_DE_ptr_A };
    instructions[0x0A] = { "LD A, (BC)", &CPU::LD_A_BC_ptr };
    instructions[0x1A] = { "LD A, (DE)", &CPU::LD_A_DE_ptr };
    instructions[0x7E] = { "LD A, (HL)", &CPU::LD_A_HL_ptr };
    instructions[0xFA] = { "LD A, [a16]", &CPU::LD_A_a16_ptr };
    instructions[0x21] = { "LD HL, n16", &CPU::LD_HL_n16 };
    instructions[0x31] = { "LD SP, n16", &CPU::LD_SP_n16 };

    instructions[0x0B] = { "DEC BC", &CPU::DEC_BC };
    instructions[0x1B] = { "DEC DE", &CPU::DEC_DE };
    instructions[0x2B] = { "DEC HL", &CPU::DEC_HL };
    instructions[0x3B] = { "DEC SP", &CPU::DEC_SP };

    instructions[0x7F] = { "LD A, A", &CPU::LD_A_A };
    instructions[0x78] = { "LD A, B", &CPU::LD_A_B };
    instructions[0x79] = { "LD A, C", &CPU::LD_A_C };
    instructions[0x7A] = { "LD A, D", &CPU::LD_A_D };
    instructions[0x7B] = { "LD A, E", &CPU::LD_A_E };
    instructions[0x7C] = { "LD A, H", &CPU::LD_A_H };
    instructions[0x7D] = { "LD A, L", &CPU::LD_A_L };

    instructions[0xB7] = { "OR A, A", &CPU::OR_A_A };
    instructions[0xB0] = { "OR A, B", &CPU::OR_A_B };
    instructions[0xB1] = { "OR A, C", &CPU::OR_A_C };
    instructions[0xB2] = { "OR A, D", &CPU::OR_A_D };
    instructions[0xB3] = { "OR A, E", &CPU::OR_A_E };
    instructions[0xB4] = { "OR A, H", &CPU::OR_A_H };
    instructions[0xB5] = { "OR A, L", &CPU::OR_A_L };
    instructions[0xB6] = { "OR A, [HL]", &CPU::OR_A_HL };
    instructions[0xF6] = { "OR A, n8", &CPU::OR_A_n8 };

    instructions[0xF5] = { "PUSH AF", &CPU::PUSH_AF };
    instructions[0xC5] = { "PUSH BC", &CPU::PUSH_BC };
    instructions[0xD5] = { "PUSH DE", &CPU::PUSH_DE };
    instructions[0xE5] = { "PUSH HL", &CPU::PUSH_HL };

    instructions[0xA7] = { "AND A, A", &CPU::AND_A_A };
    instructions[0xA0] = { "AND A, B", &CPU::AND_A_B };
    instructions[0xA1] = { "AND A, C", &CPU::AND_A_C };
    instructions[0xA2] = { "AND A, D", &CPU::AND_A_D };
    instructions[0xA3] = { "AND A, E", &CPU::AND_A_E };
    instructions[0xA4] = { "AND A, H", &CPU::AND_A_H };
    instructions[0xA5] = { "AND A, L", &CPU::AND_A_L };

    instructions[0xE6] = { "AND A, n8", &CPU::AND_A_n8 };

    instructions[0x28] = { "JR Z, e8", &CPU::JR_Z_e8 };

    instructions[0x30] = { "JR NC, e8", &CPU::JR_NC_e8 };
    instructions[0x38] = { "JR C, e8", &CPU::JR_C_e8 };

    instructions[0xC0] = { "RET NZ", &CPU::RET_NZ };
    instructions[0xC8] = { "RET Z", &CPU::RET_Z };

    instructions[0xD0] = { "RET NC", &CPU::RET_NC };
    instructions[0xD8] = { "RET C", &CPU::RET_C };

    instructions[0xE1] = { "POP HL", &CPU::POP_HL };
    instructions[0xC1] = { "POP BC", &CPU::POP_BC };
    instructions[0xD1] = { "POP DE", &CPU::POP_DE };
    instructions[0xF1] = { "POP AF", &CPU::POP_AF };

    instructions[0x2F] = { "CPL", &CPU::CPL };
    instructions[0xCB] = { "PREFIX CB", &CPU::PREFIX_CB };

    instructions[0x40] = { "LD B, B", &CPU::LD_B_B };
    instructions[0x41] = { "LD B, C", &CPU::LD_B_C };
    instructions[0x42] = { "LD B, D", &CPU::LD_B_D };
    instructions[0x43] = { "LD B, E", &CPU::LD_B_E };
    instructions[0x44] = { "LD B, H", &CPU::LD_B_H };
    instructions[0x45] = { "LD B, L", &CPU::LD_B_L };
    instructions[0x46] = { "LD B, [HL]", &CPU::LD_B_HL };
    instructions[0x47] = { "LD B, A", &CPU::LD_B_A };

    instructions[0x4F] = { "LD C, A", &CPU::LD_C_A };
    instructions[0x4E] = { "LD C, [HL]", &CPU::LD_C_HL };
    instructions[0x56] = { "LD D, [HL]", &CPU::LD_D_HL };
    instructions[0x5E] = { "LD E, [HL]", &CPU::LD_E_HL };
    instructions[0x66] = { "LD H, [HL]", &CPU::LD_H_HL };
    instructions[0x6E] = { "LD L, [HL]", &CPU::LD_L_HL };

    instructions[0x58] = { "LD E, B", &CPU::LD_E_B };
    instructions[0x59] = { "LD E, C", &CPU::LD_E_C };
    instructions[0x5A] = { "LD E, D", &CPU::LD_E_D };
    instructions[0x5B] = { "LD E, E", &CPU::LD_E_E };
    instructions[0x5C] = { "LD E, H", &CPU::LD_E_H };
    instructions[0x5D] = { "LD E, L", &CPU::LD_E_L };
    instructions[0x5F] = { "LD E, A", &CPU::LD_E_A };

    instructions[0xC7] = { "RST 00H", &CPU::RST_00 };
    instructions[0xCF] = { "RST 08H", &CPU::RST_08 };
    instructions[0xD7] = { "RST 10H", &CPU::RST_10 };
    instructions[0xDF] = { "RST 18H", &CPU::RST_18 };
    instructions[0xE7] = { "RST 20H", &CPU::RST_20 };
    instructions[0xEF] = { "RST 28H", &CPU::RST_28 };
    instructions[0xF7] = { "RST 30H", &CPU::RST_30 };
    instructions[0xFF] = { "RST 38H", &CPU::RST_38 };

    instructions[0x87] = { "ADD A, A", &CPU::ADD_A_A };
    instructions[0x80] = { "ADD A, B", &CPU::ADD_A_B };
    instructions[0x81] = { "ADD A, C", &CPU::ADD_A_C };
    instructions[0x82] = { "ADD A, D", &CPU::ADD_A_D };
    instructions[0x83] = { "ADD A, E", &CPU::ADD_A_E };
    instructions[0x84] = { "ADD A, H", &CPU::ADD_A_H };
    instructions[0x85] = { "ADD A, L", &CPU::ADD_A_L };

    instructions[0xE9] = { "JP HL", &CPU::JP_HL };
    
    instructions[0x49] = { "LD C, C", &CPU::LD_C_C };
    
    instructions[0xE2] = { "LDH [C], A", &CPU::LDH_C_ptr_A };
    instructions[0xF2] = { "LDH A, [C]", &CPU::LDH_A_C_ptr };

    instructions[0x37] = { "SCF", &CPU::SCF };
    instructions[0x3F] = { "CCF", &CPU::CCF };

    instructions[0xCE] = { "ADC A, n8", &CPU::ADC_A_n8 };
    instructions[0x8F] = { "ADC A, A", &CPU::ADC_A_A };
    instructions[0x88] = { "ADC A, B", &CPU::ADC_A_B };
    instructions[0x89] = { "ADC A, C", &CPU::ADC_A_C };
    instructions[0x8A] = { "ADC A, D", &CPU::ADC_A_D };
    instructions[0x8B] = { "ADC A, E", &CPU::ADC_A_E };
    instructions[0x8C] = { "ADC A, H", &CPU::ADC_A_H };
    instructions[0x8D] = { "ADC A, L", &CPU::ADC_A_L };
    instructions[0x8E] = { "ADC A, [HL]", &CPU::ADC_A_HL };

    instructions[0x86] = { "ADD A, [HL]", &CPU::ADD_A_HL };
    instructions[0xC6] = { "ADD A, n8", &CPU::ADD_A_n8 };

    instructions[0x97] = { "SUB A, A", &CPU::SUB_A_A };
    instructions[0x90] = { "SUB A, B", &CPU::SUB_A_B };
    instructions[0x91] = { "SUB A, C", &CPU::SUB_A_C };
    instructions[0x92] = { "SUB A, D", &CPU::SUB_A_D };
    instructions[0x93] = { "SUB A, E", &CPU::SUB_A_E };
    instructions[0x94] = { "SUB A, H", &CPU::SUB_A_H };
    instructions[0x95] = { "SUB A, L", &CPU::SUB_A_L };
    instructions[0x96] = { "SUB A, [HL]", &CPU::SUB_A_HL };
    instructions[0xD6] = { "SUB A, n8", &CPU::SUB_A_n8 };

    instructions[0x9F] = { "SBC A, A", &CPU::SBC_A_A };
    instructions[0x98] = { "SBC A, B", &CPU::SBC_A_B };
    instructions[0x99] = { "SBC A, C", &CPU::SBC_A_C };
    instructions[0x9A] = { "SBC A, D", &CPU::SBC_A_D };
    instructions[0x9B] = { "SBC A, E", &CPU::SBC_A_E };
    instructions[0x9C] = { "SBC A, H", &CPU::SBC_A_H };
    instructions[0x9D] = { "SBC A, L", &CPU::SBC_A_L };
    instructions[0x9E] = { "SBC A, [HL]", &CPU::SBC_A_HL };
    instructions[0xDE] = { "SBC A, n8", &CPU::SBC_A_n8 };

    instructions[0xA6] = { "AND A, [HL]", &CPU::AND_A_HL };
    instructions[0x6F] = { "LD L, A", &CPU::LD_L_A };
    instructions[0x69] = { "LD L, C", &CPU::LD_L_C };
    instructions[0x6B] = { "LD L, E", &CPU::LD_L_E };

    instructions[0x60] = { "LD H, B", &CPU::LD_H_B };
    instructions[0x61] = { "LD H, C", &CPU::LD_H_C };
    instructions[0x62] = { "LD H, D", &CPU::LD_H_D };
    instructions[0x63] = { "LD H, E", &CPU::LD_H_E };
    instructions[0x64] = { "LD H, H", &CPU::LD_H_H };
    instructions[0x65] = { "LD H, L", &CPU::LD_H_L };
    instructions[0x67] = { "LD H, A", &CPU::LD_H_A };

    instructions[0x54] = { "LD D, H", &CPU::LD_D_H };
    instructions[0x57] = { "LD D, A", &CPU::LD_D_A };

    instructions[0x70] = { "LD (HL), B", &CPU::LD_at_HL_B };
    instructions[0x71] = { "LD (HL), C", &CPU::LD_at_HL_C };
    instructions[0x72] = { "LD (HL), D", &CPU::LD_at_HL_D };
    instructions[0x73] = { "LD (HL), E", &CPU::LD_at_HL_E };
    instructions[0x74] = { "LD (HL), H", &CPU::LD_at_HL_H };
    instructions[0x75] = { "LD (HL), L", &CPU::LD_at_HL_L };

    instructions[0x07] = { "RLCA", &CPU::RLCA };
    instructions[0x27] = { "DAA", &CPU::DAA };
}

uint8_t CPU::XXX() {
    uint8_t opcode = mmu->read_byte(pc - 1);
    std::stringstream ss;
    ss << "Illegal/unimplemented opcode 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(opcode)
       << " at 0x" << (pc - 1);
    throw std::runtime_error("[CPU] " + ss.str());
    return 0;
}

uint8_t CPU::NOP() {
    // No operation
    return 4;
}

uint8_t CPU::JP_a16() {
    pc = mmu->read_word(pc);
    return 16;
}

uint8_t CPU::JP_NZ_a16() {
    uint16_t address = mmu->read_word(pc);
    
    if (!get_flag_z()) {
        pc = address;
        return 16;
    } else {
        pc += 2;
        return 12;
    }
}

uint8_t CPU::JP_Z_a16() {
    uint16_t address = mmu->read_word(pc);
    
    if (get_flag_z()) {
        pc = address;
        return 16;
    } else {
        pc += 2;
        return 12;
    }
}

uint8_t CPU::JP_NC_a16() {
    uint16_t address = mmu->read_word(pc);
    
    if (!get_flag_c()) {
        pc = address;
        return 16;
    } else {
        pc += 2;
        return 12;
    }
}

uint8_t CPU::JP_C_a16() {
    uint16_t address = mmu->read_word(pc);
    
    if (get_flag_c()) {
        pc = address;
        return 16;
    } else {
        pc += 2;
        return 12;
    }
}

uint8_t CPU::XOR_A_A() {
    a ^= a;
    set_flag_z(true);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::XOR_A_B() {
    a ^= b;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::XOR_A_C() {
    a ^= c;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::XOR_A_D() {
    a ^= d;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::XOR_A_E() {
    a ^= e;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::XOR_A_n8() {
    a ^= mmu->read_byte(pc++);
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return 8;
}

uint8_t CPU::LD_A_n8() {
    uint8_t value = mmu->read_byte(pc);
    pc++;

    a = value;
    return 8;
}

uint8_t CPU::LD_B_n8() {
    uint8_t value = mmu->read_byte(pc);
    pc++;

    b = value;
    return 8;
}

uint8_t CPU::LD_C_n8() {
    uint8_t value = mmu->read_byte(pc);
    pc++;

    c = value;
    return 8;
}

uint8_t CPU::LD_D_n8() {
    uint8_t value = mmu->read_byte(pc);
    pc++;

    d = value;
    return 8;
}

uint8_t CPU::LD_E_n8() {
    uint8_t value = mmu->read_byte(pc);
    pc++;

    e = value;
    return 8;
}

uint8_t CPU::LD_H_n8() {
    uint8_t value = mmu->read_byte(pc);
    pc++;

    h = value;
    return 8;
}

uint8_t CPU::LD_L_n8() {
    uint8_t value = mmu->read_byte(pc);
    pc++;

    l = value;
    return 8;
}

uint8_t CPU::LD_HL_n8() {
    uint8_t value = mmu->read_byte(pc);
    pc++;

    mmu->write_byte(get_hl(), value);
    return 12;
}

uint8_t CPU::DEC_A() {
    // Set half-carry flag if lower nibble (bit 4) is 0
    set_flag_h((a & 0x0F) == 0);

    a--;
    set_flag_z(a == 0);
    set_flag_n(true);
    return 4;
}

uint8_t CPU::DEC_B() {
    // Set half-carry flag if lower nibble (bit 4) is 0
    set_flag_h((b & 0x0F) == 0);

    b--;
    set_flag_z(b == 0);
    set_flag_n(true);
    return 4;
}

uint8_t CPU::DEC_C() {
    // Set half-carry flag if lower nibble (bit 4) is 0
    set_flag_h((c & 0x0F) == 0);

    c--;
    set_flag_z(c == 0);
    set_flag_n(true);
    return 4;
}

uint8_t CPU::JR_e8() {
    int8_t offset = static_cast<int8_t>(mmu->read_byte(pc));
    pc++;

    pc += offset;
    return 12;
}

uint8_t CPU::JR_NZ_e8() {
    int8_t offset = static_cast<int8_t>(mmu->read_byte(pc));
    pc++;

    if (!get_flag_z()) {
        pc += offset;
        return 12;
    }
    
    return 8;
}

uint8_t CPU::DI() {
    ime = false;
    return 4;
}

uint8_t CPU::EI() {
    // Uses 2 cycle delay before enabling IME
    this->ime_delay = 2; 
    return 4;
}

// TODO: I/O specific instructions - needs proper impl later
uint8_t CPU::LDH_a8_a() {
    // Get address offset
    uint8_t offset = mmu->read_byte(pc);
    pc++;

    // Write A to address 0xFF00 (beginning of I/O space) + offset
    uint16_t address = 0xFF00 + offset;
    mmu->write_byte(address, a);

    return 12;
}

uint8_t CPU::LDH_a_a8() {
    // Get address offset
    uint8_t offset = mmu->read_byte(pc);
    pc++;

    // Write value of address 0xFF00 (beginning of I/O space) + offset to register A
    uint16_t address = 0xFF00 + offset;
    a = mmu->read_byte(address);

    return 12;
}

uint8_t CPU::CP_A_n8() {
    uint8_t value = mmu->read_byte(pc);
    pc++;

    uint8_t result = a - value;

    set_flag_z(result == 0);
    set_flag_n(true);

    // Set half-carry flag if lower nibble (bit 4) is 0
    set_flag_h((a & 0x0F) < (value & 0x0F));
    set_flag_c(a < value);  

    return 8;
}

uint8_t CPU::CP_A_A() {
    uint8_t value = a;
    uint8_t result = a - value;

    set_flag_z(result == 0);
    set_flag_n(true);
    set_flag_h((a & 0x0F) < (value & 0x0F));
    set_flag_c(a < value);
    return 4;
}

uint8_t CPU::CP_A_B() {
    uint8_t value = b;
    uint8_t result = a - value;

    set_flag_z(result == 0);
    set_flag_n(true);
    set_flag_h((a & 0x0F) < (value & 0x0F));
    set_flag_c(a < value);
    return 4;
}

uint8_t CPU::CP_A_C() {
    uint8_t value = c;
    uint8_t result = a - value;

    set_flag_z(result == 0);
    set_flag_n(true);
    set_flag_h((a & 0x0F) < (value & 0x0F));
    set_flag_c(a < value);
    return 4;
}

uint8_t CPU::CP_A_D() {
    uint8_t value = d;
    uint8_t result = a - value;

    set_flag_z(result == 0);
    set_flag_n(true);
    set_flag_h((a & 0x0F) < (value & 0x0F));
    set_flag_c(a < value);
    return 4;
}

uint8_t CPU::CP_A_E() {
    uint8_t value = e;
    uint8_t result = a - value;

    set_flag_z(result == 0);
    set_flag_n(true);
    set_flag_h((a & 0x0F) < (value & 0x0F));
    set_flag_c(a < value);
    return 4;
}

uint8_t CPU::CP_A_H() {
    uint8_t value = h;
    uint8_t result = a - value;

    set_flag_z(result == 0);
    set_flag_n(true);
    set_flag_h((a & 0x0F) < (value & 0x0F));
    set_flag_c(a < value);
    return 4;
}

uint8_t CPU::CP_A_L() {
    uint8_t value = l;
    uint8_t result = a - value;

    set_flag_z(result == 0);
    set_flag_n(true);
    set_flag_h((a & 0x0F) < (value & 0x0F));
    set_flag_c(a < value);
    return 4;
}

uint8_t CPU::CP_at_HL() {
    uint8_t value = mmu->read_byte(get_hl());
    uint8_t result = a - value;

    set_flag_z(result == 0);
    set_flag_n(true);

    // Set half-carry flag if lower nibble (bit 4) is 0
    set_flag_h((a & 0x0F) < (value & 0x0F));

    // Set carry value if A < value (unsigned)
    set_flag_c(a < value);  

    return 8;
}

uint8_t CPU::CALL_a16() {
    uint16_t address = mmu->read_word(pc);
    pc += 2;

    // Push current PC to stack
    sp -= 2;
    mmu->write_word(sp, pc);

    // Jump to address
    pc = address;

    return 24;
}

uint8_t CPU::RET() {
    // Pop address from stack into PC
    pc = mmu->read_word(sp);
    sp += 2;

    return 16;
}

uint8_t CPU::RETI() {
    // Pop address from stack into PC
    pc = mmu->read_word(sp);
    sp += 2;

    ime = true;

    return 16;
}

uint8_t CPU::HALT() {
    this->halted = true;
    return 4;
}

uint8_t CPU::LD_a16_A() {
    uint16_t address = mmu->read_word(pc);
    pc += 2;

    mmu->write_byte(address, a);
    return 16;
}

uint8_t CPU::LD_BC_ptr_A() {
    mmu->write_byte(get_bc(), a);
    return 8;
}

uint8_t CPU::LD_DE_ptr_A() {
    mmu->write_byte(get_de(), a);
    return 8;
}

uint8_t CPU::LD_HL_ptr_A() {
    mmu->write_byte(get_hl(), a);
    return 8;
}

uint8_t CPU::LD_HL_ptr_inc_A() {
    uint16_t address = get_hl();
    mmu->write_byte(address, a);
    set_hl(address + 1);
    return 8;
}

uint8_t CPU::LD_HL_ptr_dec_A() {
    uint16_t address = get_hl();
    mmu->write_byte(address, a);
    set_hl(address - 1);
    return 8;
}

uint8_t CPU::LD_a16_SP() {
    uint16_t address = mmu->read_word(pc);
    pc += 2;

    mmu->write_word(address, sp);
    return 20;
}

uint8_t CPU::LDH_C_A() {
    uint16_t address = 0xFF00 + c;
    mmu->write_byte(address, a);
    return 8;
}

uint8_t CPU::INC_A() {
    // Set half-carry flag if lower nibble (bit 4) is 0x0F before increment
    set_flag_h((a & 0x0F) == 0x0F);
    a++;

    set_flag_z(a == 0);
    set_flag_n(false);
    return 4;
}

uint8_t CPU::INC_B() {
    // Set half-carry flag if lower nibble (bit 4) is 0x0F before increment
    set_flag_h((b & 0x0F) == 0x0F);
    b++;

    set_flag_z(b == 0);
    set_flag_n(false);
    return 4;
}

uint8_t CPU::INC_C() {
    // Set half-carry flag if lower nibble (bit 4) is 0x0F before increment
    set_flag_h((c & 0x0F) == 0x0F);
    c++;

    set_flag_z(c == 0);
    set_flag_n(false);
    return 4;
}

uint8_t CPU::INC_D() {
    // Set half-carry flag if lower nibble (bit 4) is 0x0F before increment
    set_flag_h((d & 0x0F) == 0x0F);
    d++;

    set_flag_z(d == 0);
    set_flag_n(false);
    return 4;
}

uint8_t CPU::INC_E() {
    // Set half-carry flag if lower nibble (bit 4) is 0x0F before increment
    set_flag_h((e & 0x0F) == 0x0F);
    e++;

    set_flag_z(e == 0);
    set_flag_n(false);
    return 4;
}

uint8_t CPU::INC_H() {
    // Set half-carry flag if lower nibble (bit 4) is 0x0F before increment
    set_flag_h((h & 0x0F) == 0x0F);
    h++;

    set_flag_z(h == 0);
    set_flag_n(false);
    return 4;
}

uint8_t CPU::INC_L() {
    // Set half-carry flag if lower nibble (bit 4) is 0x0F before increment
    set_flag_h((l & 0x0F) == 0x0F);
    l++;

    set_flag_z(l == 0);
    set_flag_n(false);
    return 4;
}

uint8_t CPU::INC_at_HL() {
    uint16_t address = get_hl();
    uint8_t value = mmu->read_byte(address);

    // Half-carry: set if bit 3 overflows into bit 4
    set_flag_h((value & 0x0F) == 0x0F);
    value++;

    set_flag_z(value == 0);
    set_flag_n(false);

    mmu->write_byte(address, value);

    return 12;
}

uint8_t CPU::LD_BC_n16() {
    uint16_t value = mmu->read_word(pc);
    pc += 2;

    set_bc(value);
    return 12;
}

uint8_t CPU::LD_DE_n16() {
    uint16_t value = mmu->read_word(pc);
    pc += 2;

    set_de(value);
    return 12;
}

uint8_t CPU::LD_HL_n16() {
    uint16_t value = mmu->read_word(pc);
    pc += 2;

    set_hl(value);
    return 12;
}

uint8_t CPU::LD_SP_n16() {
    uint16_t value = mmu->read_word(pc);
    pc += 2;

    sp = value;
    return 12;
}

uint8_t CPU::DEC_BC() {
    uint16_t bc = get_bc();
    bc--;
    set_bc(bc);
    return 8;
}

uint8_t CPU::DEC_DE() {
    uint16_t de = get_de();
    de--;
    set_de(de);
    return 8;
}

uint8_t CPU::DEC_HL() {
    uint16_t hl = get_hl();
    hl--;
    set_hl(hl);
    return 8;
}

uint8_t CPU::DEC_SP() {
    sp--;
    return 8;
}

uint8_t CPU::DEC_D() {
    set_flag_h((d & 0x0F) == 0);
    d--;
    set_flag_z(d == 0);
    set_flag_n(true);
    return 4;
}

uint8_t CPU::DEC_E() {
    set_flag_h((e & 0x0F) == 0);
    e--;
    set_flag_z(e == 0);
    set_flag_n(true);
    return 4;
}

uint8_t CPU::DEC_H() {
    set_flag_h((h & 0x0F) == 0);
    h--;
    set_flag_z(h == 0);
    set_flag_n(true);
    return 4;
}

uint8_t CPU::DEC_L() {
    set_flag_h((l & 0x0F) == 0);
    l--;
    set_flag_z(l == 0);
    set_flag_n(true);
    return 4;
}

uint8_t CPU::DEC_at_HL() {
    uint16_t address = get_hl();
    uint8_t value = mmu->read_byte(address);

    set_flag_h((value & 0x0F) == 0);
    value--;
    set_flag_z(value == 0);
    set_flag_n(true);

    mmu->write_byte(address, value);
    return 12;
}

uint8_t CPU::LD_A_B() {
    a = b;
    return 4;
}

uint8_t CPU::LD_A_C() {
    a = c;
    return 4;
}

uint8_t CPU::LD_A_D() {
    a = d;
    return 4;
}

uint8_t CPU::LD_A_E() {
    a = e;
    return 4;
}

uint8_t CPU::LD_A_H() {
    a = h;
    return 4;
}

uint8_t CPU::LD_A_L() {
    a = l;
    return 4;
}

uint8_t CPU::LD_A_A() {
    // NOP equivalent, here for consistency
    return 4;
}

uint8_t CPU::OR_A_A() {
    a |= a;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);

    return 4;
}

uint8_t CPU::OR_A_B() {
    a |= b;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::OR_A_C() {
    a |= c;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::OR_A_D() {
    a |= d;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::OR_A_E() {
    a |= e;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::OR_A_H() {
    a |= h;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::OR_A_L() {
    a |= l;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::OR_A_HL() {
    a |= mmu->read_byte(get_hl());
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return 8;
}

uint8_t CPU::OR_A_n8() {
    uint8_t value = mmu->read_byte(pc);
    pc++;

    a |= value;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);

    return 8;
}

uint8_t CPU::PUSH_AF() {
    sp -= 2;
    mmu->write_word(sp, get_af());
    return 16;
}

uint8_t CPU::PUSH_BC() {
    sp -= 2;
    mmu->write_word(sp, get_bc());
    return 16;
}

uint8_t CPU::PUSH_DE() {
    sp -= 2;
    mmu->write_word(sp, get_de());
    return 16;
}

uint8_t CPU::PUSH_HL() {
    sp -= 2;
    mmu->write_word(sp, get_hl());
    return 16;
}

uint8_t CPU::AND_A_A() {
    a &= a;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(true);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::AND_A_B() {
    a &= b;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(true);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::AND_A_C() {
    a &= c;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(true);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::AND_A_D() {
    a &= d;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(true);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::AND_A_E() {
    a &= e;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(true);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::AND_A_H() {
    a &= h;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(true);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::AND_A_L() {
    a &= l;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(true);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::AND_A_n8() {
    uint8_t value = mmu->read_byte(pc);
    pc++;

    a &= value;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(true);
    set_flag_c(false);

    return 8;
}

uint8_t CPU::JR_Z_e8() {
    int8_t offset = static_cast<int8_t>(mmu->read_byte(pc));
    pc++;

    if (get_flag_z()) {
        pc += offset;
        return 12;
    }
    
    return 8;
}

uint8_t CPU::JR_C_e8() {
    int8_t offset = static_cast<int8_t>(mmu->read_byte(pc));
    pc++;

    if (get_flag_c()) {
        pc += offset;
        return 12;
    }
    
    return 8;
}

uint8_t CPU::JR_NC_e8() {
    int8_t offset = static_cast<int8_t>(mmu->read_byte(pc));
    pc++;

    if (!get_flag_c()) {
        pc += offset;
        return 12;
    }
    
    return 8;
}

uint8_t CPU::RET_NZ() {
    if (!get_flag_z()) {
        // Pop address from stack into PC
        pc = mmu->read_word(sp);
        sp += 2;
        return 20;
    }

    return 8;
}

uint8_t CPU::RET_Z() {
    if (get_flag_z()) {
        // Pop address from stack into PC
        pc = mmu->read_word(sp);
        sp += 2;
        return 20;
    }

    return 8;
}

uint8_t CPU::RET_NC() {
    if (!get_flag_c()) {
        // Pop address from stack into PC
        pc = mmu->read_word(sp);
        sp += 2;

        return 20;
    } else {
        return 8;
    }
}

uint8_t CPU::RET_C() {
    if (get_flag_c()) {
        // Pop address from stack into PC
        pc = mmu->read_word(sp);
        sp += 2;

        return 20;
    } else {
        return 8;
    }
}

uint8_t CPU::LD_A_BC_ptr() {
    a = mmu->read_byte(get_bc());
    return 8;
}

uint8_t CPU::LD_A_DE_ptr() {
    a = mmu->read_byte(get_de());
    return 8;
}

uint8_t CPU::LD_A_HL_ptr() {
    a = mmu->read_byte(get_hl());
    return 8;
}

uint8_t CPU::LD_A_a16_ptr() {
    uint16_t address = mmu->read_word(pc);
    pc += 2;

    a = mmu->read_byte(address);

    return 16;
}

uint8_t CPU::LD_A_HL_ptr_inc() {
    uint16_t address = get_hl();
    a = mmu->read_byte(address);
    set_hl(address + 1);
    return 8;
}

uint8_t CPU::LD_A_HL_ptr_dec() {
    uint16_t address = get_hl();
    a = mmu->read_byte(address);
    set_hl(address - 1);
    return 8;
}

uint8_t CPU::POP_HL() {
    uint16_t value = mmu->read_word(sp);
    sp += 2;
    set_hl(value);
    return 12;
}

uint8_t CPU::POP_BC() {
    uint16_t value = mmu->read_word(sp);
    sp += 2;
    set_bc(value);
    return 12;
}

uint8_t CPU::POP_DE() {
    uint16_t value = mmu->read_word(sp);
    sp += 2;
    set_de(value);
    return 12;
}

uint8_t CPU::POP_AF() {
    uint16_t value = mmu->read_word(sp);
    sp += 2;
    set_af(value);
    return 12;
}

uint8_t CPU::CPL() {
    a = ~a;
    set_flag_n(true);
    set_flag_h(true);
    return 4;
}

uint8_t CPU::PREFIX_CB() {
    uint8_t cb_opcode = mmu->read_byte(pc);
    pc++;

    // Execute the instruction from the CB-specific table
    return execute_cb_instruction(cb_opcode);
}

uint8_t CPU::LD_B_B() {
    // Redundant load (NOP operation equivalent)
    return 4;
}

uint8_t CPU::LD_B_C() {
    b = c;
    return 4;
}

uint8_t CPU::LD_B_D() {
    b = d;
    return 4;
}

uint8_t CPU::LD_B_E() {
    b = e;
    return 4;
}

uint8_t CPU::LD_B_H() {
    b = h;
    return 4;
}

uint8_t CPU::LD_B_L() {
    b = l;
    return 4;
}

uint8_t CPU::LD_B_A() {
    b = a;
    return 4;
}

uint8_t CPU::LD_C_A() {
    c = a;
    return 4;
}

uint8_t CPU::LD_E_B() {
    e = b;
    return 4;
}

uint8_t CPU::LD_E_C() {
    e = c;
    return 4;
}

uint8_t CPU::LD_E_D() {
    e = d;
    return 4;
}

uint8_t CPU::LD_E_E() {
    // Equivalent to NOP, but added here for consistency
    return 4;
}

uint8_t CPU::LD_E_H() {
    e = h;
    return 4;
}

uint8_t CPU::LD_E_L() {
    e = l;
    return 4;
}

uint8_t CPU::LD_B_HL() {
    b = mmu->read_byte(get_hl());
    return 8;
}

uint8_t CPU::LD_C_HL() {
    c = mmu->read_byte(get_hl());
    return 8;
}

uint8_t CPU::LD_D_HL() {
    d = mmu->read_byte(get_hl());
    return 8;
}

uint8_t CPU::LD_E_HL() {
    e = mmu->read_byte(get_hl());
    return 8;
}

uint8_t CPU::LD_H_HL() {
    uint16_t address = get_hl();

    // Read the byte from memory and update H. After this line, the HL pair will point to a different location.
    h = mmu->read_byte(address);

    return 8;
}

uint8_t CPU::LD_L_HL() {
    uint16_t address = get_hl();

    // Read the byte from memory and update L. After this line, the HL pair will point to a different location.
    l = mmu->read_byte(address);

    return 8;
}

uint8_t CPU::LD_E_A() {
    e = a;
    return 4;
}

uint8_t CPU::RST_00() {
    sp -= 2;
    mmu->write_word(sp, pc);
    pc = 0x0000;
    return 16;
}

uint8_t CPU::RST_08() {
    sp -= 2;
    mmu->write_word(sp, pc);
    pc = 0x0008;
    return 16;
}

uint8_t CPU::RST_10() {
    sp -= 2;
    mmu->write_word(sp, pc);
    pc = 0x0010;
    return 16;
}

uint8_t CPU::RST_18() {
    sp -= 2;
    mmu->write_word(sp, pc);
    pc = 0x0018;
    return 16;
}

uint8_t CPU::RST_20() {
    sp -= 2;
    mmu->write_word(sp, pc);
    pc = 0x0020;
    return 16;
}

uint8_t CPU::RST_28() {
    sp -= 2;
    mmu->write_word(sp, pc);
    pc = 0x0028;
    return 16;
}

uint8_t CPU::RST_30() {
    sp -= 2;
    mmu->write_word(sp, pc);
    pc = 0x0030;
    return 16;
}

uint8_t CPU::RST_38() {
    sp -= 2;
    mmu->write_word(sp, pc);
    pc = 0x0038;
    return 16;
}

uint8_t CPU::ADD_A_A() {
    uint8_t val = a; // Adding A to A
    
    // Half-carry - Carry from bit 3 to bit 4
    set_flag_h(((a & 0x0F) + (val & 0x0F)) > 0x0F);
    
    // Carry - Carry from bit 7 (result > 0xFF)
    set_flag_c((static_cast<uint16_t>(a) + static_cast<uint16_t>(val)) > 0xFF);

    a += val;

    set_flag_z(a == 0);
    set_flag_n(false);

    return 4;
}

uint8_t CPU::ADD_A_B() {
    uint8_t val = b; // Adding B to A
    
    // Half-carry - Carry from bit 3 to bit 4
    set_flag_h(((a & 0x0F) + (val & 0x0F)) > 0x0F);
    
    // Carry - Carry from bit 7 (result > 0xFF)
    set_flag_c((static_cast<uint16_t>(a) + static_cast<uint16_t>(val)) > 0xFF);

    a += val;

    set_flag_z(a == 0);
    set_flag_n(false);

    return 4;
}

uint8_t CPU::ADD_A_C() {
    uint8_t val = c; // Adding C to A
    
    // Half-carry - Carry from bit 3 to bit 4
    set_flag_h(((a & 0x0F) + (val & 0x0F)) > 0x0F);
    
    // Carry - Carry from bit 7 (result > 0xFF)
    set_flag_c((static_cast<uint16_t>(a) + static_cast<uint16_t>(val)) > 0xFF);

    a += val;

    set_flag_z(a == 0);
    set_flag_n(false);

    return 4;
}

uint8_t CPU::ADD_A_D() {
    uint8_t val = d; // Adding D to A
    
    // Half-carry - Carry from bit 3 to bit 4
    set_flag_h(((a & 0x0F) + (val & 0x0F)) > 0x0F);
    
    // Carry - Carry from bit 7 (result > 0xFF)
    set_flag_c((static_cast<uint16_t>(a) + static_cast<uint16_t>(val)) > 0xFF);

    a += val;

    set_flag_z(a == 0);
    set_flag_n(false);

    return 4;
}

uint8_t CPU::ADD_A_E() {
    uint8_t val = e; // Adding E to A
    
    // Half-carry - Carry from bit 3 to bit 4
    set_flag_h(((a & 0x0F) + (val & 0x0F)) > 0x0F);
    
    // Carry - Carry from bit 7 (result > 0xFF)
    set_flag_c((static_cast<uint16_t>(a) + static_cast<uint16_t>(val)) > 0xFF);

    a += val;

    set_flag_z(a == 0);
    set_flag_n(false);

    return 4;
}

uint8_t CPU::ADD_A_H() {
    uint8_t val = h; // Adding D to A
    
    // Half-carry - Carry from bit 3 to bit 4
    set_flag_h(((a & 0x0F) + (val & 0x0F)) > 0x0F);
    
    // Carry - Carry from bit 7 (result > 0xFF)
    set_flag_c((static_cast<uint16_t>(a) + static_cast<uint16_t>(val)) > 0xFF);

    a += val;

    set_flag_z(a == 0);
    set_flag_n(false);

    return 4;
}

uint8_t CPU::ADD_A_L() {
    uint8_t val = l; // Adding L to A
    
    // Half-carry - Carry from bit 3 to bit 4
    set_flag_h(((a & 0x0F) + (val & 0x0F)) > 0x0F);
    
    // Carry - Carry from bit 7 (result > 0xFF)
    set_flag_c((static_cast<uint16_t>(a) + static_cast<uint16_t>(val)) > 0xFF);

    a += val;

    set_flag_z(a == 0);
    set_flag_n(false);

    return 4;
}

uint8_t CPU::ADD_HL_BC() {
    uint16_t hl_val = get_hl();
    uint16_t bc_val = get_bc();
    uint32_t result = hl_val + bc_val;

    // Half-carry (16-bit) - carry from bit 11 to bit 12
    set_flag_h(((hl_val & 0x0FFF) + (bc_val & 0x0FFF)) > 0x0FFF);

    // Carry - carry from bit 15 to bit 16 (result > 0xFFFF)
    set_flag_c(result > 0xFFFF);

    set_flag_n(false);

    // Zero flag is not affected
    set_hl(static_cast<uint16_t>(result));
    
    return 8;
}

uint8_t CPU::ADD_HL_DE() {
    uint16_t hl_val = get_hl();
    uint16_t de_val = get_de();
    uint32_t result = hl_val + de_val;

    // Half-carry (16-bit) - carry from bit 11 to bit 12
    set_flag_h(((hl_val & 0x0FFF) + (de_val & 0x0FFF)) > 0x0FFF);

    // Carry - carry from bit 15 to bit 16 (result > 0xFFFF)
    set_flag_c(result > 0xFFFF);

    set_flag_n(false);

    // Zero flag is not affected
    set_hl(static_cast<uint16_t>(result));
    
    return 8;
}

uint8_t CPU::ADD_HL_HL() {
    uint16_t hl_val = get_hl();
    // Adding HL to itself
    uint32_t result = hl_val + hl_val;

    // Half-carry (16-bit) - carry from bit 11 to bit 12
    set_flag_h(((hl_val & 0x0FFF) + (hl_val & 0x0FFF)) > 0x0FFF);

    // Carry - carry from bit 15 to bit 16 (result > 0xFFFF)
    set_flag_c(result > 0xFFFF);

    set_flag_n(false);

    // Zero flag is not affected
    set_hl(static_cast<uint16_t>(result));
    
    return 8;
}

uint8_t CPU::ADD_HL_SP() {
    uint16_t hl_val = get_hl();
    uint16_t sp_val = sp;
    uint32_t result = hl_val + sp_val;

    // Half-carry (16-bit) - carry from bit 11 to bit 12
    set_flag_h(((hl_val & 0x0FFF) + (sp_val & 0x0FFF)) > 0x0FFF);

    // Carry - carry from bit 15 to bit 16 (result > 0xFFFF)
    set_flag_c(result > 0xFFFF);

    set_flag_n(false);

    // Zero flag is not affected
    set_hl(static_cast<uint16_t>(result));
    
    return 8;
}

uint8_t CPU::INC_BC() {
    set_bc(get_bc() + 1);
    return 8;
}

uint8_t CPU::INC_DE() {
    set_de(get_de() + 1);
    return 8;
}

uint8_t CPU::INC_HL() {
    set_hl(get_hl() + 1);
    return 8;
}

uint8_t CPU::INC_SP() {
    sp++;
    return 8;
}

uint8_t CPU::JP_HL() {
    pc = get_hl();
    return 4;
}

uint8_t CPU::LD_C_C() {
    // Equivalent to NOP, but added here for consistency
    return 4;
}

uint8_t CPU::LDH_A_C_ptr() {
    // Address to read is in IO space plus value of register C
    uint16_t address = 0xFF00 + c;
    a = mmu->read_byte(address);

    return 8;
}

uint8_t CPU::LDH_C_ptr_A() {
    // Address to read is in IO space plus value of register C
    uint16_t address = 0xFF00 + c;
    mmu->write_byte(address, a);

    return 8;
}

uint8_t CPU::SCF() {
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(true);
    return 4;
}

uint8_t CPU::CCF() {
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(!get_flag_c());
    return 4;
}

void CPU::alu_add(uint8_t val, bool carry) {
    uint16_t c = carry ? 1 : 0;
    uint16_t result = a + val + c;
    
    set_flag_z((result & 0xFF) == 0);
    set_flag_n(false);
    // Half-carry: overflow from bit 3
    set_flag_h(((a & 0x0F) + (val & 0x0F) + c) > 0x0F);
    // Carry: overflow from bit 7
    set_flag_c(result > 0xFF);
    
    a = static_cast<uint8_t>(result);
}

void CPU::alu_sub(uint8_t val, bool carry) {
    uint16_t c = carry ? 1 : 0;
    int16_t result = a - val - c;
    
    set_flag_z((result & 0xFF) == 0);
    set_flag_n(true);
    // Half-carry: borrow from bit 4
    set_flag_h(((a & 0x0F) - (val & 0x0F) - c) < 0);
    // Carry: borrow from bit 8 (result < 0)
    set_flag_c(result < 0);
    
    a = static_cast<uint8_t>(result);
}

uint8_t CPU::AND_A_HL() {
    uint8_t val = mmu->read_byte(get_hl());
    a &= val;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(true);
    set_flag_c(false);
    return 8;
}

uint8_t CPU::ADD_A_HL() {
    alu_add(mmu->read_byte(get_hl()), false);
    return 8;
}

uint8_t CPU::ADD_A_n8() {
    alu_add(mmu->read_byte(pc++), false);
    return 8;
}

uint8_t CPU::ADC_A_A() { alu_add(a, get_flag_c()); return 4; }
uint8_t CPU::ADC_A_B() { alu_add(b, get_flag_c()); return 4; }
uint8_t CPU::ADC_A_C() { alu_add(c, get_flag_c()); return 4; }
uint8_t CPU::ADC_A_D() { alu_add(d, get_flag_c()); return 4; }
uint8_t CPU::ADC_A_E() { alu_add(e, get_flag_c()); return 4; }
uint8_t CPU::ADC_A_H() { alu_add(h, get_flag_c()); return 4; }
uint8_t CPU::ADC_A_L() { alu_add(l, get_flag_c()); return 4; }
uint8_t CPU::ADC_A_HL() { alu_add(mmu->read_byte(get_hl()), get_flag_c()); return 8; }
uint8_t CPU::ADC_A_n8() { alu_add(mmu->read_byte(pc++), get_flag_c()); return 8; }

uint8_t CPU::SUB_A_A() { alu_sub(a, false); return 4; }
uint8_t CPU::SUB_A_B() { alu_sub(b, false); return 4; }
uint8_t CPU::SUB_A_C() { alu_sub(c, false); return 4; }
uint8_t CPU::SUB_A_D() { alu_sub(d, false); return 4; }
uint8_t CPU::SUB_A_E() { alu_sub(e, false); return 4; }
uint8_t CPU::SUB_A_H() { alu_sub(h, false); return 4; }
uint8_t CPU::SUB_A_L() { alu_sub(l, false); return 4; }
uint8_t CPU::SUB_A_HL() { alu_sub(mmu->read_byte(get_hl()), false); return 8; }
uint8_t CPU::SUB_A_n8() { alu_sub(mmu->read_byte(pc++), false); return 8; }

uint8_t CPU::SBC_A_A() { alu_sub(a, get_flag_c()); return 4; }
uint8_t CPU::SBC_A_B() { alu_sub(b, get_flag_c()); return 4; }
uint8_t CPU::SBC_A_C() { alu_sub(c, get_flag_c()); return 4; }
uint8_t CPU::SBC_A_D() { alu_sub(d, get_flag_c()); return 4; }
uint8_t CPU::SBC_A_E() { alu_sub(e, get_flag_c()); return 4; }
uint8_t CPU::SBC_A_H() { alu_sub(h, get_flag_c()); return 4; }
uint8_t CPU::SBC_A_L() { alu_sub(l, get_flag_c()); return 4; }
uint8_t CPU::SBC_A_HL() { alu_sub(mmu->read_byte(get_hl()), get_flag_c()); return 8; }
uint8_t CPU::SBC_A_n8() { alu_sub(mmu->read_byte(pc++), get_flag_c()); return 8; }

uint8_t CPU::LD_L_A() {
    l = a;
    return 4;
}

uint8_t CPU::LD_L_C() {
    l = c;
    return 4;
}

uint8_t CPU::LD_L_E() {
    l = e;
    return 4;
}

uint8_t CPU::LD_H_A() {
    h = a;
    return 4;
}

uint8_t CPU::LD_H_B() {
    h = b;
    return 4;
}

uint8_t CPU::LD_H_C() {
    h = c;
    return 4;
}

uint8_t CPU::LD_H_D() {
    h = d;
    return 4;
}

uint8_t CPU::LD_H_E() {
    h = e;
    return 4;
}

uint8_t CPU::LD_H_H() {
    // NOP equivalent, here for consistency
    return 4;
}

uint8_t CPU::LD_H_L() {
    h = l;
    return 4;
}

uint8_t CPU::LD_D_A() {
    d = a;
    return 4;
}

uint8_t CPU::LD_D_H() {
    d = h;
    return 4;
}

uint8_t CPU::LD_at_HL_B() {
    uint16_t address = get_hl();
    mmu->write_byte(address, b);
    
    return 8;
}

uint8_t CPU::LD_at_HL_C() {
    uint16_t address = get_hl();
    mmu->write_byte(address, c);
    
    return 8;
}

uint8_t CPU::LD_at_HL_D() {
    uint16_t address = get_hl();
    mmu->write_byte(address, d);
    
    return 8;
}

uint8_t CPU::LD_at_HL_E() {
    uint16_t address = get_hl();
    mmu->write_byte(address, e);
    
    return 8;
}

uint8_t CPU::LD_at_HL_H() {
    uint16_t address = get_hl();
    mmu->write_byte(address, h);
    
    return 8;
}

uint8_t CPU::LD_at_HL_L() {
    uint16_t address = get_hl();
    mmu->write_byte(address, l);
    
    return 8;
}

uint8_t CPU::RLCA() {
    // Find bit 7 and rotate it
    uint8_t bit7 = (a & 0x80) >> 7;
    a = (a << 1) | bit7;

    set_flag_z(false);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(bit7 == 1);

    return 4;
}

uint8_t CPU::DAA() {
    uint8_t adjustment = 0;
    bool new_carry = false;

    if (!get_flag_n()) {
        // After an ADD operation
        if (get_flag_c() || a > 0x99) {
            adjustment |= 0x60;
            new_carry = true;
        }
        if (get_flag_h() || (a & 0x0F) > 0x09) {
            adjustment |= 0x06;
        }
    } else {
        // After a SUB operation
        if (get_flag_c()) {
            adjustment |= 0x60;
            new_carry = true;
        }
        if (get_flag_h()) {
            adjustment |= 0x06;
        }
        
        // Subtract the adjustment instead of adding it
        a -= adjustment;
    }

    if (!get_flag_n()) {
        a += adjustment;
    }

    set_flag_z(a == 0);
    set_flag_h(false);
    set_flag_c(new_carry);

    return 4;
}