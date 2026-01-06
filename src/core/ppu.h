#pragma once
#include "mmu.h"
#include <SDL3/SDL.h>

class PPU {
    public:
        MMU* mmu = nullptr;

        // Connect instance of MMU to read VRAM
        void connect_mmu(MMU* m);

        // Initalize SDL3 components
        void init_sdl();

        // Render frame from framebuffer
        void render_frame();

        // Tick PPU with given CPU cycles
        void tick(uint8_t cycles);

        // Get/reset internal scanline values
        uint8_t get_ly() const { return current_ly; }
        void reset_ly() { current_ly = 0; ppu_cycles = 0; }
    private:
        // SDL components
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        SDL_Texture* texture = nullptr;

        // Basic PPU functions

        // Raw pixel data (160x144 pixels)
        uint32_t framebuffer[160 * 144];

        // Current value of LY (scanline) - avoid conflict between CPU and PPU reads/writes
        uint8_t current_ly = 0;
        
        // Cycle count for PPU timing
        uint16_t ppu_cycles;

        // Initial mode on startup is OAM search
        uint8_t mode = 2;

        // Read VRAM and fill frame buffer
        void draw_scanline();

        // Request interrupt
        void request_interrupt(uint8_t bit);
};