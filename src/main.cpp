#include <iostream>
#include <SDL3/SDL.h>
#include <string>

#include "core/cpu.h"
#include "core/mmu.h"
#include "core/rom.h"
#include "core/ppu.h"
#include "core/joypad.h"

// Structure to hold file dialog state
struct DialogState {
    bool complete = false;
    std::string selected_path;
};

// Callback function for file dialog
void SDLCALL file_dialog_callback(void* userdata, const char* const* filelist, int filter_count) {
    DialogState* state = static_cast<DialogState*>(userdata);
    if (filelist && *filelist) {
        // filelist is a null-terminated array of strings
        state->selected_path = *filelist;
    }
    state->complete = true;
}

// Constants for timing
const int CYCLES_PER_FRAME = 70224;
// 4194304 Hz / 70224 cycles/frame = 59.7275 Hz
const double FRAME_TIME_MS = 1000.0 / 59.7275; 

int main(int argc, char* argv[]) {
    // Base components and connections
    MMU mmu;
    CPU cpu;
    PPU ppu;
    ROM rom;
    Joypad joypad;

    ppu.connect_mmu(&mmu);
    mmu.connect_ppu(&ppu);
    cpu.connect_mmu(&mmu);
    mmu.connect_cpu(&cpu);
    mmu.connect_joypad(&joypad);

    // Initialization
    std::cout << "[GameByte] Initializing GameByte..." << std::endl;

    // Initialize SDL3, throw error if it fails
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        std::cerr << "[SDL] Failed to initialize - SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Initialize PPU SDL components
    ppu.init_sdl();

    bool running = true;
    SDL_Event e;
    bool frame_drawn_this_vblank = false;

    // Open file dialog to select ROM file
    DialogState dialog_state;
    const SDL_DialogFileFilter filters[] = {
        { "Game Boy ROM", "gb" }
    };

    SDL_ShowOpenFileDialog(file_dialog_callback, &dialog_state, nullptr, filters, 1, ".", false);

    // Wait for the dialog to be closed
    while (!dialog_state.complete) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) return 0;
        }
        SDL_Delay(10);
    }

    // If user cancelled the dialog, close app
    if (dialog_state.selected_path.empty()) {
        return 0;
    }

    // Attempt to load ROM from path
    if (ROM::load(dialog_state.selected_path.c_str())) {
        mmu.load_game(ROM::data, ROM::size);
    } else {
        std::string error_msg = "Failed to load ROM: " + dialog_state.selected_path;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GameByte - Initialization Error", error_msg.c_str(), nullptr);
        return 1;
    }

    // Main emulation loop
    uint32_t frame_count = 0;
    while (running) {
        frame_count++;

        // Debug - initial VRAM dump
        // if (frame_count == 60) {
        //     mmu.dump_vram();
        // }
        
        uint64_t start_time = SDL_GetTicks();
        int cycles_this_frame = 0;
        int cycles_since_last_poll = 0;

        // Run CPU for one frame
        try {
            while (cycles_this_frame < CYCLES_PER_FRAME) {
                int cycles = cpu.step();
                cycles_this_frame += cycles;
                cycles_since_last_poll += cycles;
                
                cpu.tick_timers(cycles);
                ppu.tick(cycles);

                // Poll for input every scanline (~456 cycles)
                if (cycles_since_last_poll >= 456) {
                    while (SDL_PollEvent(&e) != 0) {
                        // Handle quit event
                        if (e.type == SDL_EVENT_QUIT) {
                            running = false;
                        }

                        // Input handoff from SDL to Joypad
                        if (joypad.handle_sdl_event(e)) {
                            // Request Joypad Interrupt (bit 4 of IF register)
                            uint8_t if_reg = mmu.read_byte(0xFF0F);
                            mmu.write_byte(0xFF0F, if_reg | 0x10);
                        }
                    }
                    cycles_since_last_poll = 0;
                }

                // Check if frame is ready to be drawn
                if (ppu.get_ly() == 144) {
                    if (!frame_drawn_this_vblank) {
                        ppu.render_frame();
                        frame_drawn_this_vblank = true;
                    }
                } else if (ppu.get_ly() != 144) {
                    // Only allow a new draw once the PPU leaves the V-Blank trigger line
                    frame_drawn_this_vblank = false;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[GameByte] Emulation error about to occur. Total cycles we got through: " << cpu.total_cycles << std::endl;
            std::cerr << e.what() << std::endl;
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GameByte - Execution Error", e.what(), nullptr);
            running = false; // Stop on error
            return 1;
        }

        // Debug keys
        const bool* keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_F1]) {
            mmu.dump_vram();
        }

        if (keys[SDL_SCANCODE_F2]) {
            mmu.dump_hram();
        }

        if (keys[SDL_SCANCODE_F3]) {
            cpu.debug_interrupt_status();
        }

        if (keys[SDL_SCANCODE_F4]) {
            cpu.dump_history();
        }

        // Timing synchronization
        uint64_t end_time = SDL_GetTicks();
        double elapsed_ms = static_cast<double>(end_time - start_time);
        
        if (elapsed_ms < FRAME_TIME_MS) {
            // Sleep for the remaining time
            SDL_Delay(static_cast<uint32_t>(FRAME_TIME_MS - elapsed_ms));
        }
    }

    SDL_Quit();
    return 0;
}
