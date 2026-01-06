#include <iostream>
#include <SDL3/SDL.h>
#include "core/cpu.h"
#include "core/mmu.h"
#include "core/rom.h"

// Constants for timing
const int CYCLES_PER_FRAME = 70224;
// 4194304 Hz / 70224 cycles/frame = 59.7275 Hz
const double FRAME_TIME_MS = 1000.0 / 59.7275; 

int main(int argc, char* argv[]) {
    // Initialize SDL3, throw error if it fails
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        std::cerr << "[SDL] Failed to initialize - SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Base components
    MMU mmu;
    CPU cpu;
    cpu.connect_mmu(&mmu);
    ROM rom;

    std::cout << "[GameByte] Initializing GameByte..." << std::endl;

    bool running = true;
    SDL_Event e;

    if (ROM::load("tetris.gb")) {
        mmu.load_game(ROM::data, ROM::size);
    } else {
        std::cerr << "[GameByte] Failed to load ROM!" << std::endl;
        // Optionally exit or continue
    }

    while (running) {
        uint64_t start_time = SDL_GetTicks();
        int cycles_this_frame = 0;

        // Handle events
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        // Run CPU for one frame
        while (cycles_this_frame < CYCLES_PER_FRAME) {
            try {
                int cycles = cpu.step();
                cycles_this_frame += cycles;
            } catch (const std::exception& e) {
                std::cerr << "[GameByte] Emulation error about to occur. Total cycles we got through: " << cpu.total_cycles << std::endl;
                std::cerr << e.what() << std::endl;
                running = false; // Stop on error
                break;
            }
        }

        // Emulate other components here (PPU, Timer, etc.) based on cycles passed?
        // Typically PPU catches up or runs interleaved. 
        // For now, just CPU timing is requested.

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
