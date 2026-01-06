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
    cpu.connect_mmu(&mmu);
    mmu.connect_cpu(&cpu);
    ROM rom;
    PPU ppu;
    ppu.connect_mmu(&mmu);
    mmu.connect_ppu(&ppu);

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
    while (running) {
        uint64_t start_time = SDL_GetTicks();
        int cycles_this_frame = 0;

        // Handle SDL events
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        // Temp breakpoint for tile render event in Tetris
        if (cpu.pc == 0x2817) {
            std::cout << "[GameByte] Reached PC 0x2817 (first tile render event in Tetris), stopping emulation for debugging." << std::endl;
            running = false;
        }

        // Run CPU for one frame
        while (cycles_this_frame < CYCLES_PER_FRAME) {
            try {
                // Initalize cycle count and check for halting
                int cycles = 0;
                if (!cpu.halted) {
                    cycles = cpu.step();
                } else {
                    cycles = 4;
                }

                cycles += cpu.handle_interrupts();
                cycles_this_frame += cycles;
                
                cpu.tick_timers(cycles);
                ppu.tick(cycles);
            } catch (const std::exception& e) {
                std::cerr << "[GameByte] Emulation error about to occur. Total cycles we got through: " << cpu.total_cycles << std::endl;
                std::cerr << e.what() << std::endl;
                running = false; // Stop on error
                break;
            }
        }

        // Render frame
        ppu.render_frame();

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
