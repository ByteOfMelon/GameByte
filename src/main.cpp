#include <iostream>
#include <SDL3/SDL.h>
#include "core/cpu.h"
#include "core/mmu.h"
#include "core/rom.h"
#include "core/ppu.h"

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

    ppu.connect_mmu(&mmu);
    mmu.connect_ppu(&ppu);
    cpu.connect_mmu(&mmu);
    mmu.connect_cpu(&cpu);

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

    if (ROM::load("tetris.gb")) {
        mmu.load_game(ROM::data, ROM::size);
    } else {
        std::cerr << "[GameByte] Failed to load ROM!" << std::endl;
        // Optionally exit or continue
    }

    // Main emulation loop
    uint32_t frame_count = 0;
    while (running) {
        frame_count++;

        // Debug - initial VRAM dump
        if (frame_count == 60) {
            mmu.dump_vram();
        }
        
        uint64_t start_time = SDL_GetTicks();
        int cycles_this_frame = 0;

        // Handle SDL events
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
                
                cpu.tick_timers(cycles);
                ppu.tick(cycles);

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
            } catch (const std::exception& e) {
                std::cerr << "[GameByte] Emulation error about to occur. Total cycles we got through: " << cpu.total_cycles << std::endl;
                std::cerr << e.what() << std::endl;
                running = false; // Stop on error
                break;
            }
        }

        // Debug key
        const bool* keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_F1]) {
            mmu.dump_vram();
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
