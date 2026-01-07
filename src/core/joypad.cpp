#include "joypad.h"

uint8_t Joypad::get_joyp_state() {
    uint8_t res = 0xC0 | control_mask;
    uint8_t buttons = 0x0F;

    // Direction keys selected (Bit 4 is 0)
    if (!(control_mask & 0x10)) {
        buttons &= direction_buttons;
    }

    // Action keys selected (Bit 5 is 0)
    if (!(control_mask & 0x20)) {
        buttons &= action_buttons;
    }
    
    return (res & 0xF0) | (buttons & 0x0F);
}

bool Joypad::handle_sdl_event(const SDL_Event& e) {
    if (e.type != SDL_EVENT_KEY_DOWN && e.type != SDL_EVENT_KEY_UP) return false;

    bool pressed = (e.type == SDL_EVENT_KEY_DOWN);
    SDL_Keycode key = e.key.key;
    bool interrupt_needed = false;

    auto update_bit = [&](uint8_t& buttons, int bit, bool is_pressed) {
        if (is_pressed) {
            // If the button was previously NOT pressed (bit was 1), trigger interrupt
            if (buttons & (1 << bit)) interrupt_needed = true;
            buttons &= ~(1 << bit);
        } else {
            buttons |= (1 << bit);
        }
    };

    switch (key) {
        // Directions
        case SDLK_RIGHT:  update_bit(direction_buttons, 0, pressed); break;
        case SDLK_LEFT:   update_bit(direction_buttons, 1, pressed); break;
        case SDLK_UP:     update_bit(direction_buttons, 2, pressed); break;
        case SDLK_DOWN:   update_bit(direction_buttons, 3, pressed); break;

        // Actions
        case SDLK_Z:      update_bit(action_buttons, 0, pressed);    break; // A
        case SDLK_X:      update_bit(action_buttons, 1, pressed);    break; // B
        case SDLK_RSHIFT: update_bit(action_buttons, 2, pressed);    break; // Select
        case SDLK_RETURN: update_bit(action_buttons, 3, pressed);    break; // Start

        default: break;
    }

    return interrupt_needed;
}