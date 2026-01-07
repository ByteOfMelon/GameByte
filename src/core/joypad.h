#pragma once
#include <cstdint>
#include <SDL3/SDL.h>

class Joypad {
    public:
        // Button states (0 = pressed, 1 = released)
        uint8_t action_buttons = 0x0F;    // Start, Select, B, A
        uint8_t direction_buttons = 0x0F; // Down, Up, Left, Right
        
        // The value written by the CPU to $FF00 to select which buttons to read
        uint8_t control_mask = 0x30; 

        // Get current state of the joypad register ($FF00)
        uint8_t get_joyp_state();

        // Handles SDL events and returns true if a Joypad Interrupt (bit 4) should be requested
        bool handle_sdl_event(const SDL_Event& event);
};