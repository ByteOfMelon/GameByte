#pragma once
#include "mmu.h"
#include <SDL3/SDL.h>

class PPU {
    public:
        PPU(); 

        MMU* mmu = nullptr;

        // Connect instance of MMU to read VRAM
        void connect_mmu(MMU* m);

        // Initalize SDL3 components
        void init_sdl();

        // Render frame from framebuffer
        void render_frame();

        // Render a blank (white) frame (used when LCD is disabled)
        void render_blank();

        // Tick PPU with given CPU cycles
        void tick(uint8_t cycles);

        // Get/reset internal scanline values
        uint8_t get_ly() const { return current_ly; }
        void reset_ly() { current_ly = 0; ppu_cycles = 0; }

        // General register getters/setters
        uint8_t get_lcdc() const { return lcdc; }
        void set_lcdc(uint8_t value) { lcdc = value; }

        uint8_t get_stat() const { return stat; }
        void set_stat(uint8_t value) { stat = (value & 0x78) | (stat & 0x07); }

        uint8_t get_scy() const { return scy; }
        void set_scy(uint8_t value) { scy = value; }

        uint8_t get_scx() const { return scx; }
        void set_scx(uint8_t value) { scx = value; }

        uint8_t get_lyc() const { return lyc; }
        void set_lyc(uint8_t value) { lyc = value; }

        uint8_t get_bgp() const { return bgp; }
        void set_bgp(uint8_t value) { bgp = value; }
    private:
        // SDL components
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        SDL_Texture* texture = nullptr;

        // Basic PPU functions

        // Raw pixel data (160x144 pixels)
        uint32_t framebuffer[160 * 144];

        // General hardware registers
        uint8_t lcdc, stat, scy, scx, lyc, bgp;

        // Current value of LY (scanline) - avoid conflict between CPU and PPU reads/writes
        uint8_t current_ly = 0;
        
        // Cycle count for PPU timing
        uint16_t ppu_cycles;

        // Initial mode on startup is OAM search
        uint8_t mode = 2;

        // Check last mode for STAT interrupt triggering
        uint8_t last_mode = 255;

        // Read VRAM and fill frame buffer
        void draw_scanline();

        // Request interrupt
        void request_interrupt(uint8_t bit);
};