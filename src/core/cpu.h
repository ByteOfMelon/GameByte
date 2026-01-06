#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "mmu.h"

/**
 * @brief Emulates the Game Boy's CPU, specifically the Sharp SM83.
 * 
 * This is an 8-bit CPU that runs at 4.194304 MHz, uses a modified Z80 instruction set, and contains the following registers:
 * 
 * 8-bit General Purpose - A (Accumulator), then B, C, D, E, H and L registers.
 *
 * 16-bit Register Pairs - Combines two 8-bit registers to make 16-bit values. Can be used for pointers or 16-bit operations.
 * 
 * 16-bit Special Purpose Registers - includes the stack pointer (points to current top of stack in RAM) and the program counter (points to address of next instruction in RAM)
 * 
 * Flag Register (F) - 8-bit register where specific bits indicate outcome of arithmetic/logical operations.
 * 
 * Bit 7: Zero Flag (Z) - Set if the result of an operation is zero.
 * Bit 6: Subtract Flag (N) - Set if the last operation was a subtraction.
 * Bit 5: Half Carry Flag (H) - Set if there was a carry from bit 3 to bit 4 during an operation (used for BCD arithmetic).
 * Bit 4: Carry Flag (C) - Set if an operation resulted in a carry out of bit 7 (for additions) or a borrow (for subtractions).
 * Bits 3-0: Not used, always read as zero.
 * 
 * Additionally, the CPU has several interupts: VBlank, LCD STAT, Timer, Serial, and Joypad. The CPU pauses execution upon any of these interupts being triggered, pushes the program counter (PC) onto the stack, and jumps to a specific vector address in RAM.
 */
class CPU {
    public:
        // External modules
        MMU* mmu = nullptr;
        uint32_t total_cycles = 0;

        // Instruction handling
        struct Instruction {
            const char* name;
            uint8_t (CPU::*operate)();
        };

        std::vector<Instruction> instructions;

        // 8-bit general purpose registers
        uint8_t a, b, c, d, e, h, l;

        // Flag register
        uint8_t f;

        // 16-bit registers
        uint16_t sp; // Stack pointer
        uint16_t pc; // Program counter

        // Interrupt Master Enable flag
        bool ime;

        // Constructor
        CPU();

        /**
         * Getter/setter methods for 16-bit register pairs
         */
        uint16_t get_af() const;
        void set_af(uint16_t value);
        uint16_t get_bc() const;
        void set_bc(uint16_t value);
        uint16_t get_de() const;
        void set_de(uint16_t value);
        uint16_t get_hl() const;
        void set_hl(uint16_t value);

        /**
         * Getter/setter methods for flags
         */
        bool get_flag_z() const;
        void set_flag_z(bool value);
        bool get_flag_n() const;
        void set_flag_n(bool value);
        bool get_flag_h() const;
        void set_flag_h(bool value);
        bool get_flag_c() const;
        void set_flag_c(bool value);

        // Connect an initialized MMU to the CPU
        void connect_mmu(MMU* mmu);

        // Execute the next instruction
        // Returns the number of cycles consumed
        uint8_t step();

        // Initialize instruction table for opcode handling
        void init_instructions();

        // Opcode Implementations

        // Illegal/unimplemented opcode
        uint8_t XXX();

        // No operation (0x00)
        uint8_t NOP();

        // Jump to 16-bit absolute address (0xC3)
        uint8_t JP_a16();

        // XOR A with A (0xAF)
        uint8_t XOR_a();

        // Load 16-bit immediate into HL register pair (0x21)
        uint8_t LD_HL_n16();

        // Load 8-bit immediate into C register (0x0E)
        uint8_t LD_C_n8();

        // Load 8-bit immediate into B register (0x06)
        uint8_t LD_B_n8();

        // Load 16-bit immediate into stack pointer (0x31)
        uint8_t LD_SP_n16();

        // Load the value of register A into memory address pointed to by HL, then decrement HL (0x32)
        uint8_t LD_HLmA_dec();
        
        // Decrement register B (0x05)
        uint8_t DEC_B();

        // Decrement register C (0x0D)
        uint8_t DEC_C();

        // Jump relative if zero flag not set (0x20)
        uint8_t JR_NZ_e8();

        // Load 8-bit immediate into register A (0x3E)
        uint8_t LD_A_n8();
        
        // Disable master interupt flag (0xF3)
        uint8_t DI();

        // Enable master interupt flag (0xFB)
        uint8_t EI();
};
