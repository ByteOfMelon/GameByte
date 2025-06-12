#include <cstdint>

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
        // 8-bit general purpose registers
        uint8_t a, b, c, d, e, h, l;

        // Flag register
        uint8_t f;

        // 16-bit registers
        uint16_t sp; // Stack pointer
        uint16_t pc; // Program counter

        /**
         * Getter/setter methods for 16-bit register pairs
         */

        // AF register pair
        uint16_t get_af() const { 
            return (static_cast<uint16_t>(a) << 8) | f;
        }

        void set_af(uint16_t value) {
            a = (value >> 8) & 0xFF; f = value & 0xF0; // Lower 4 bits of F are always 0
        }

        // BC register pair
        uint16_t get_bc() const { 
            return (static_cast<uint16_t>(b) << 8) | c;
        }

        void set_bc(uint16_t value) {
            b = (value >> 8) & 0xFF; c = value & 0xFF;
        }

        // DE register pair
        uint16_t get_de() const { 
            return (static_cast<uint16_t>(d) << 8) | e;
        }

        void set_de(uint16_t value) {
            d = (value >> 8) & 0xFF; e = value & 0xFF;
        }

        // HL register pair
        uint16_t get_hl() const { 
            return (static_cast<uint16_t>(h) << 8) | l;
        }

        void set_hl(uint16_t value) {
            h = (value >> 8) & 0xFF; l = value & 0xFF;
        }

        /**
         * Getter/setter methods for flags
         */

        // Z flag (zero flag) - if result of operation was zero
        bool get_flag_z() const { 
            return (f & 0x80) != 0; 
        }

        void set_flag_z(bool value) { 
            f = (value)? (f | 0x80) : (f & ~0x80);
        }

        // N flag (subtract flag) - if result of operation was a subtraction
        bool get_flag_n() const { 
            return (f & 0x40) != 0; 
        }

        void set_flag_n(bool value) { 
            f = (value)? (f | 0x40) : (f & ~0x40);
        }

        // H flag (half carry flag) - if there was a carry from bit 3 to bit 4 during an operation
        bool get_flag_h() const { 
            return (f & 0x20) != 0; 
        }

        void set_flag_h(bool value) { 
            f = (value)? (f | 0x20) : (f & ~0x20);
        }

        // C flag (carry flag) - if an operation resulted in a carry out of bit 7 (for additions) or a borrow (for subtractions).
        bool get_flag_c() const { 
            return (f & 0x10) != 0; 
        }

        void set_flag_c(bool value) { 
            f = (value)? (f | 0x10) : (f & ~0x10);
        }
};