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

        // Interrupt Master Enable flags
        bool ime;
        int ime_delay = 0;

        // Halted flag
        bool halted = false;

        // Internal counter for div timer
        uint16_t internal_counter;

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

        // Tick internal CPU timers based on cycles passed
        void tick_timers(uint8_t cycles);

        // Reset internal counter
        void reset_internal_counter();

        // Interrupt handlers
        uint8_t handle_interrupts();
        uint8_t execute_interrupt(uint8_t bit, uint16_t vector);

        // Execute the next instruction
        // Returns the number of cycles consumed
        uint8_t step();

        // Debug the status of interupts
        void debug_interrupt_status();

        // Implement extended opcodes (0xCB prefix)
        uint8_t execute_cb_instruction(uint8_t opcode);

        // Handle carry flag for CB instructions
        uint8_t handle_cb_shift_rotate(uint8_t opcode, uint8_t value);

        // Initialize instruction table for opcode handling
        void init_instructions();

        // Opcode Implementations

        // Illegal/unimplemented opcode
        uint8_t XXX();

        // No operation (0x00)
        uint8_t NOP();

        // Jump to 16-bit absolute address (0xC3)
        uint8_t JP_a16();

        // Jump to 16-bit absolute address if zero flag not set (0xC2)
        uint8_t JP_NZ_a16();

        // Jump to 16-bit absolute address if zero flag is set (0xCA)
        uint8_t JP_Z_a16();

        // Jump to 16-bit absolute address if carry flag not set (0xD2)
        uint8_t JP_NC_a16();

        // Jump to 16-bit absolute address if carry flag is set (0xDA)
        uint8_t JP_C_a16();

        // XOR A with A (0xAF)
        uint8_t XOR_A_A();

        // XOR A with B (0xA8)
        uint8_t XOR_A_B();

        // XOR A with C (0xA9)
        uint8_t XOR_A_C();

        // XOR A with D (0xAA)
        uint8_t XOR_A_D();

        // XOR A with E (0xAB)
        uint8_t XOR_A_E();

        // XOR A with 8-bit immediate (0xEE)
        uint8_t XOR_A_n8();

        // Load 8-bit immediate group
        uint8_t LD_A_n8(); // 0x3E
        uint8_t LD_B_n8(); // 0x06
        uint8_t LD_C_n8(); // 0x0E
        uint8_t LD_D_n8(); // 0x16
        uint8_t LD_E_n8(); // 0x1E
        uint8_t LD_H_n8(); // 0x26
        uint8_t LD_L_n8(); // 0x2E
        uint8_t LD_HL_n8(); // 0x36
        
        // Decrement register A by 1 (0x3D)
        uint8_t DEC_A();

        // Decrement register B (0x05)
        uint8_t DEC_B();

        // Decrement register C (0x0D)
        uint8_t DEC_C();

        // Decrement register D (0x15)
        uint8_t DEC_D();

        // Decrement register E (0x1D)
        uint8_t DEC_E();

        // Decrement register H (0x25)
        uint8_t DEC_H();

        // Decrement register L (0x2D)
        uint8_t DEC_L();

        // Decrement value pointed to by HL (0x35)
        uint8_t DEC_at_HL();

        // Jump to relative address (0x18)
        uint8_t JR_e8();

        // Jump relative if zero flag not set (0x20)
        uint8_t JR_NZ_e8();

        // Disable master interupt flag (0xF3)
        uint8_t DI();

        // Enable master interupt flag (0xFB)
        uint8_t EI();

        // Load value of register A into high I/O memory address specified by 8-bit immediate (0xE0)
        uint8_t LDH_a8_a();

        // Load value of high I/O memory address specified by 8-bit immediate into register A (0xF0)
        uint8_t LDH_a_a8();

        // Compare family
        uint8_t CP_A_n8(); // 0xFE
        uint8_t CP_A_A();  // 0xBF
        uint8_t CP_A_B();  // 0xB8
        uint8_t CP_A_C();  // 0xB9
        uint8_t CP_A_D();  // 0xBA
        uint8_t CP_A_E();  // 0xBB
        uint8_t CP_A_H();  // 0xBC
        uint8_t CP_A_L();  // 0xBD
        uint8_t CP_at_HL(); // 0xBE

        // Call 16-bit immediate address (0xCD)
        uint8_t CALL_a16();

        // Return from subroutine (0xC9)
        uint8_t RET();

        // Return from subroutine and set IME (0xD9)
        uint8_t RETI();

        // Halt CPU until next interrupt (0x76)
        uint8_t HALT();

        // Write value of register A to 16-bit immediate (0xEA)
        uint8_t LD_a16_A();

        // Store A Indirect Group
        uint8_t LD_BC_ptr_A(); // 0x02
        uint8_t LD_DE_ptr_A(); // 0x12
        uint8_t LD_HL_ptr_A(); // 0x77
        uint8_t LD_HL_ptr_inc_A(); // 0x22
        uint8_t LD_HL_ptr_dec_A(); // 0x32
        
        // Store SP to 16-bit immediate address (0x08)
        uint8_t LD_a16_SP();

        // Write value of register A to the I/O address specified by current C register (0xE2)
        uint8_t LDH_C_A();

        // 8-bit Increment Group
        uint8_t INC_A(); // 0x3C
        uint8_t INC_B(); // 0x04
        uint8_t INC_C(); // 0x0C
        uint8_t INC_D(); // 0x14
        uint8_t INC_E(); // 0x1C
        uint8_t INC_H(); // 0x24
        uint8_t INC_L(); // 0x2C
        uint8_t INC_at_HL(); // 0x34

        // Load 16-bit immediate into register pair group
        uint8_t LD_BC_n16(); // 0x01
        uint8_t LD_DE_n16(); // 0x11
        uint8_t LD_HL_n16(); // 0x21
        uint8_t LD_SP_n16(); // 0x31

        // Decrement BC register pair by 1 (0x0B)
        uint8_t DEC_BC();

        // Decrement DE register pair by 1 (0x1B)
        uint8_t DEC_DE();

        // Decrement HL register pair by 1 (0x2B)
        uint8_t DEC_HL();

        // Decrement SP register pair by 1 (0x3B)
        uint8_t DEC_SP();

        // Copy 8-bit value from B register to A register (0x78)
        uint8_t LD_A_B();

        // Copy value from C register to A register (0x79)
        uint8_t LD_A_C();

        // Copy value from D register to A register (0x7A)
        uint8_t LD_A_D();

        // Copy value from E register to A register (0x7B)
        uint8_t LD_A_E();

        // Copy value from H register to A register (0x7C)
        uint8_t LD_A_H();

        // Copy value from L register to A register (0x7D)
        uint8_t LD_A_L();
        
        // Copy 8-bit value from A register to A register (0x7F)
        uint8_t LD_A_A();

        // Bitwise OR with A and B register, result in A (0xB0)
        uint8_t OR_A_B();

        // Bitwise OR with A and C register, result in A (0xB1)
        uint8_t OR_A_C();
        
        // Bitwise OR with A and D register, result in A (0xB2)
        uint8_t OR_A_D();

        // Bitwise OR with A and E register, result in A (0xB3)
        uint8_t OR_A_E();

        // Bitwise OR with A and H register, result in A (0xB4)
        uint8_t OR_A_H();

        // Bitwise OR with A and L register, result in A (0xB5)
        uint8_t OR_A_L();

        // Bitwise OR with A and value of address pointed to by HL, result in A (0xB6)
        uint8_t OR_A_HL();

        // Bitwise OR with A and 8-bit immediate, result in A (0xF6)
        uint8_t OR_A_n8();

        // Push AF register pair to stack (0xF5)
        uint8_t PUSH_AF();

        // Push BC register pair to stack (0xC5)
        uint8_t PUSH_BC();

        // Push DE register pair to stack (0xD5)
        uint8_t PUSH_DE();

        // Push HL register pair to stack (0xE5)
        uint8_t PUSH_HL();

        // Bitwise AND between A and A register, result in A (0xA7)
        uint8_t AND_A_A();

        // Bitwise AND between A and B register, result in A (0xA0)
        uint8_t AND_A_B();

        // Bitwise AND between A and C register, result in A (0xA1)
        uint8_t AND_A_C();

        // Bitwise AND between A and D register, result in A (0xA2)
        uint8_t AND_A_D();
        
        // Bitwise AND between A and E register, result in A (0xA3)
        uint8_t AND_A_E();

        // Bitwise AND between A and H register, result in A (0xA4)
        uint8_t AND_A_H();

        // Bitwise AND between A and L register, result in A (0xA5)
        uint8_t AND_A_L();

        // Bitwise AND between A and 8-bit immediate, result in A (0xE6)
        uint8_t AND_A_n8();

        // Jump relative if zero flag set (0x28)
        uint8_t JR_Z_e8();

        // Return from subroutine if zero flag not set (0xC0)
        uint8_t RET_NZ();

        // Return from subroutine if zero flag set (0xC8)
        uint8_t RET_Z();

        // Load A Indirect Group
        uint8_t LD_A_BC_ptr(); // 0x0A
        uint8_t LD_A_DE_ptr(); // 0x1A
        uint8_t LD_A_HL_ptr(); // 0x7E
        uint8_t LD_A_a16_ptr(); // 0xFA
        uint8_t LD_A_HL_ptr_inc(); // 0x2A
        uint8_t LD_A_HL_ptr_dec(); // 0x3A

        // Pop HL register pair from stack (0xE1)
        uint8_t POP_HL();

        // Pop BC register pair from stack (0xC1)
        uint8_t POP_BC();

        // Pop DE register pair from stack (0xD1)
        uint8_t POP_DE();

        // Pop AF register pair from stack (0xF1)
        uint8_t POP_AF();

        // Bitwise NOT on A register (0x2F)
        uint8_t CPL();
        
        // Extended opcode prefix for specialized instructions (0xCB)
        uint8_t PREFIX_CB();

        // Load Register B Group
        uint8_t LD_B_B(); // 0x40
        uint8_t LD_B_C(); // 0x41
        uint8_t LD_B_D(); // 0x42
        uint8_t LD_B_E(); // 0x43
        uint8_t LD_B_H(); // 0x44
        uint8_t LD_B_L(); // 0x45
        uint8_t LD_B_A(); // 0x47

        // Copy A register value into C register (0x4F)
        uint8_t LD_C_A();

        // Load r, [HL] Group
        uint8_t LD_B_HL(); // 0x46
        uint8_t LD_C_HL(); // 0x4E
        uint8_t LD_D_HL(); // 0x56
        uint8_t LD_E_HL(); // 0x5E
        uint8_t LD_H_HL(); // 0x66
        uint8_t LD_L_HL(); // 0x6E

        // Load Register E Group
        uint8_t LD_E_B(); // 0x58
        uint8_t LD_E_C(); // 0x59
        uint8_t LD_E_D(); // 0x5A
        uint8_t LD_E_E(); // 0x5B
        uint8_t LD_E_H(); // 0x5C
        uint8_t LD_E_L(); // 0x5D
        uint8_t LD_E_A(); // 0x5F

        // "Restarts" / calls to fixed addresses
        uint8_t RST_00(); // 0xC7
        uint8_t RST_08(); // 0xCF
        uint8_t RST_10(); // 0xD7
        uint8_t RST_18(); // 0xDF
        uint8_t RST_20(); // 0xE7
        uint8_t RST_28(); // 0xEF
        uint8_t RST_30(); // 0xF7
        uint8_t RST_38(); // 0xFF

        // Add A and A register, result in A (0x87)
        uint8_t ADD_A_A();

        // Add A and B register, result in A (0x80)
        uint8_t ADD_A_B();

        // Add A and C register, result in A (0x81)
        uint8_t ADD_A_C();

        // Add A and D register, result in A (0x82)
        uint8_t ADD_A_D();

        // Add A and E register, result in A (0x83)
        uint8_t ADD_A_E();

        // Add A and H register, result in A (0x84)
        uint8_t ADD_A_H();

        // Add A and L register, result in A (0x85)
        uint8_t ADD_A_L();
        uint8_t ADD_A_HL(); // 0x86
        uint8_t ADD_A_n8(); // 0xC6

        // ADC Group
        uint8_t ADC_A_A(); // 0x8F
        uint8_t ADC_A_B(); // 0x88
        uint8_t ADC_A_C(); // 0x89
        uint8_t ADC_A_D(); // 0x8A
        uint8_t ADC_A_E(); // 0x8B
        uint8_t ADC_A_H(); // 0x8C
        uint8_t ADC_A_L(); // 0x8D
        uint8_t ADC_A_HL(); // 0x8E
        uint8_t ADC_A_n8(); // 0xCE

        // SUB Group
        uint8_t SUB_A_A(); // 0x97
        uint8_t SUB_A_B(); // 0x90
        uint8_t SUB_A_C(); // 0x91
        uint8_t SUB_A_D(); // 0x92
        uint8_t SUB_A_E(); // 0x93
        uint8_t SUB_A_H(); // 0x94
        uint8_t SUB_A_L(); // 0x95
        uint8_t SUB_A_HL(); // 0x96
        uint8_t SUB_A_n8(); // 0xD6

        // SBC Group
        uint8_t SBC_A_A(); // 0x9F
        uint8_t SBC_A_B(); // 0x98
        uint8_t SBC_A_C(); // 0x99
        uint8_t SBC_A_D(); // 0x9A
        uint8_t SBC_A_E(); // 0x9B
        uint8_t SBC_A_H(); // 0x9C
        uint8_t SBC_A_L(); // 0x9D
        uint8_t SBC_A_HL(); // 0x9E
        uint8_t SBC_A_n8(); // 0xDE

        // AND Indirect
        uint8_t AND_A_HL(); // 0xA6

        // 16-bit ADD Instructions
        uint8_t ADD_HL_BC(); // 0x09
        uint8_t ADD_HL_DE(); // 0x19
        uint8_t ADD_HL_HL(); // 0x29
        uint8_t ADD_HL_SP(); // 0x39

        // 16-bit INC Instructions
        uint8_t INC_BC(); // 0x03
        uint8_t INC_DE(); // 0x13
        uint8_t INC_HL(); // 0x23
        uint8_t INC_SP(); // 0x33

        // Jump to address in HL (0xE9)
        uint8_t JP_HL();

        // Copy value of register C into C register (0x49)
        uint8_t LD_C_C();

        // Copy value of I/O address pointed to by C register into A register (0xF2)
        uint8_t LDH_A_C_ptr();

        // Copy value of A register into I/O address pointed to by C register (0xE2)
        uint8_t LDH_C_ptr_A();

        // Force carry flag to be 1, reset N and H flags (0x37)
        uint8_t SCF();

        // Flip carry flag bit, reset N and H flags (0x3F)
        uint8_t CCF();

        // Load into L register group
        uint8_t LD_L_A(); // 0x6F
        uint8_t LD_L_C(); // 0x69
        uint8_t LD_L_E(); // 0x6B

        // Load into register H group
        uint8_t LD_H_A(); // 0x67
        uint8_t LD_H_B(); // 0x60
        uint8_t LD_H_C(); // 0x61
        uint8_t LD_H_D(); // 0x62
        uint8_t LD_H_E(); // 0x63
        uint8_t LD_H_H(); // 0x64
        uint8_t LD_H_L(); // 0x65

        // Load into D register group 
        uint8_t LD_D_A(); // 0x57
        uint8_t LD_D_H(); // 0x54

        // Write value of register B into address pointed to by HL (0x70)
        uint8_t LD_at_HL_B();

        // Write value of register C into address pointed to by HL (0x71)
        uint8_t LD_at_HL_C();

        // Write value of register D into address pointed to by HL (0x72)
        uint8_t LD_at_HL_D();
        
        // Write value of register E into address pointed to by HL (0x73)
        uint8_t LD_at_HL_E();
        
        // Write value of register H into address pointed to by HL (0x74)
        uint8_t LD_at_HL_H();
        
        // Write value of register L into address pointed to by HL (0x75)
        uint8_t LD_at_HL_L();
    private:
        // Performs addition (ADD/ADC) and updates flags
        // carry: if true, adds the C flag to the sum
        void alu_add(uint8_t val, bool carry);

        // Helper: Performs Subtraction (SUB/SBC) and updates flags
        // carry: if true, subtracts the C flag from the result
        void alu_sub(uint8_t val, bool carry);
};
