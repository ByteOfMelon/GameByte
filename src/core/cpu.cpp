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
    
    ime = false; // Interrupts disabled by default
}

uint16_t CPU::get_af() const { 
    return (static_cast<uint16_t>(a) << 8) | f;
}

void CPU::set_af(uint16_t value) {
    a = (value >> 8) & 0xFF; f = value & 0xF0;
}

uint16_t CPU::get_bc() const { 
    return (static_cast<uint16_t>(b) << 8) | c;
}

void CPU::set_bc(uint16_t value) {
    b = (value >> 8) & 0xFF; c = value & 0xFF;
}

uint16_t CPU::get_de() const { 
    return (static_cast<uint16_t>(d) << 8) | e;
}

void CPU::set_de(uint16_t value) {
    d = (value >> 8) & 0xFF; e = value & 0xFF;
}

uint16_t CPU::get_hl() const { 
    return (static_cast<uint16_t>(h) << 8) | l;
}

void CPU::set_hl(uint16_t value) {
    h = (value >> 8) & 0xFF; l = value & 0xFF;
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
}

uint8_t CPU::step() {
    if (!mmu) {
        throw std::runtime_error("[CPU] MMU was not connected to CPU before execution");
    }

    uint8_t opcode = mmu->read_byte(pc);
    printf("[CPU] DEBUG: Executing opcode 0x%02X (instruction %s) at address 0x%04X\n", opcode, instructions[opcode].name, pc);
    pc++;

    uint8_t cycles = (this->*instructions[opcode].operate)();

    total_cycles += cycles;
    return cycles;
}

void CPU::init_instructions() {
    instructions.assign(256, { "XXX", &CPU::XXX });
    instructions[0x00] = { "NOP", &CPU::NOP };
    instructions[0xC3] = { "JP a16", &CPU::JP_a16 };
    instructions[0xAF] = { "XOR A, A", &CPU::XOR_a };
    instructions[0x21] = { "LD HL, n16", &CPU::LD_HL_n16 };
    instructions[0x0E] = { "LD C, n8", &CPU::LD_C_n8 };
    instructions[0x06] = { "LD B, n8", &CPU::LD_B_n8 };
    instructions[0x31] = { "LD SP, n16", &CPU::LD_SP_n16 };
    instructions[0x32] = { "LD (HL-), A", &CPU::LD_HLmA_dec };
    instructions[0x05] = { "DEC B", &CPU::DEC_B };
    instructions[0x0D] = { "DEC C", &CPU::DEC_C };
    instructions[0x20] = { "JR NZ, e8", &CPU::JR_NZ_e8 };
    instructions[0x3E] = { "LD A, n8", &CPU::LD_A_n8 };
    instructions[0xF3] = { "DI", &CPU::DI };
    instructions[0xFB] = { "EI", &CPU::EI };
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

uint8_t CPU::XOR_a() {
    a ^= a;
    set_flag_z(a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return 4;
}

uint8_t CPU::LD_HL_n16() {
    uint16_t value = mmu->read_word(pc);
    set_hl(value);
    pc += 2;
    return 12;
}

uint8_t CPU::LD_C_n8() {
    uint8_t value = mmu->read_byte(pc);
    c = value;
    pc++;
    return 8;
}

uint8_t CPU::LD_B_n8() {
    uint8_t value = mmu->read_byte(pc);
    b = value;
    pc++;
    return 8;
}

uint8_t CPU::LD_SP_n16() {
    uint16_t value = mmu->read_word(pc);
    sp = value;
    pc += 2;
    return 12;
}

uint8_t CPU::LD_HLmA_dec() {
    uint16_t address = get_hl();
    mmu->write_byte(address, a);
    set_hl(address - 1);
    return 8;
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

uint8_t CPU::JR_NZ_e8() {
    int8_t offset = static_cast<int8_t>(mmu->read_byte(pc));
    pc++;

    if (!get_flag_z()) {
        pc += offset;
        return 12;
    }
    
    return 8;
}

uint8_t CPU::LD_A_n8() {
    uint8_t value = mmu->read_byte(pc);
    a = value;
    pc++;
    return 8;
}

uint8_t CPU::DI() {
    ime = false;
    return 4;
}

uint8_t CPU::EI() {
    ime = true;
    return 4;
}