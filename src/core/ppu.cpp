#include "ppu.h"

void PPU::connect_mmu(MMU* m) {
    mmu = m;
}

void PPU::init_sdl() {
    SDL_Init(SDL_INIT_VIDEO);

    // Initialize the window. Render at 2x scale for visibility
    window = SDL_CreateWindow("GameByte", (160*2), (144*2), 0);
    renderer = SDL_CreateRenderer(window, nullptr);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);
}

void PPU::render_frame() {
    SDL_UpdateTexture(texture, NULL, framebuffer, 160 * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void PPU::tick(uint8_t cycles) {
    ppu_cycles += cycles;

    switch (mode) {
        // OAM search (80 cycles)
        case 2: 
            if (ppu_cycles >= 80) {
                ppu_cycles -= 80;
                mode = 3;
            }
            break;
        
        // Pixel transfer (172 cycles)
        case 3:
            if (ppu_cycles >= 172) {
                ppu_cycles -= 172;
                mode = 0;
                draw_scanline(); // Draw the current line at the end of transfer
            }
            break;
        
        // H-blank (204 cycles)
        case 0:
            if (ppu_cycles >= 204) {
                ppu_cycles -= 204;
                current_ly++;

                if (current_ly == 144) {
                    mode = 1; 
                    request_interrupt(0); // V-blank Interrupt
                } else {
                    mode = 2; 
                }
            }
            break;
        
        // V-blank (456 cycles per line, 10 lines total)
        case 1:
            if (ppu_cycles >= 456) {
                ppu_cycles -= 456;
                current_ly++;
                
                if (current_ly > 153) {
                    current_ly = 0; // Reset to start of next frame
                    mode = 2;
                }
            }
            break;
    }
}

void PPU::draw_scanline() {
    // Get current scanline position
    uint8_t ly = current_ly;

    // Check if scanline is beyond visible area
    if (ly >= 144) return;

    // Get the background palette (BGP) at 0xFF47
    uint8_t bgp = mmu->read_byte(0xFF47);
    uint32_t shades[] = { 0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000 };

    // Get scroll (x and y) pos
    uint8_t scy = mmu->read_byte(0xFF42);
    uint8_t scx = mmu->read_byte(0xFF43);

    // Identify which tile map to use (from LCDC bit 3)
    uint16_t map_base = (mmu->read_byte(0xFF40) & 0x08) ? 0x9C00 : 0x9800;
    
    // Find the vertical tile row
    uint8_t y_pos = ly + scy;
    uint16_t tile_row = (y_pos / 8) * 32;

    for (int px = 0; px < 160; px++) {
        uint8_t x_pos = px + scx;
        uint16_t tile_col = (x_pos / 8);
        
        // Get tile index from the map
        uint8_t tile_index = mmu->read_byte(map_base + tile_row + tile_col);

        // Find the tile data address ($8000 area)

        // Identify tile data addressing mode (LCDC bit 4), handles signed addressing
        bool is_unsigned = (mmu->read_byte(0xFF40) & 0x10);
        uint16_t tile_data_addr;

        if (is_unsigned) {
            tile_data_addr = 0x8000 + (tile_index * 16);
        } else {
            // Signed addressing - 0x9000 is the base, tile_index is treated as a signed 8-bit int
            tile_data_addr = 0x9000 + (static_cast<int8_t>(tile_index) * 16);
        }
        
        // Get the specific 2 bytes for the 8 pixels in this row
        uint8_t line = (y_pos % 8) * 2;
        uint8_t byte1 = mmu->read_byte(tile_data_addr + line);
        uint8_t byte2 = mmu->read_byte(tile_data_addr + line + 1);

        // Extract the 2 bits for the current pixel
        int bit = 7 - (x_pos % 8);
        uint8_t color_id = ((byte2 >> bit) & 0x01) << 1 | ((byte1 >> bit) & 0x01);

        // Map the 2-bit color_id through the BGP palette - each 2 bits of BGP represent a color for IDs 0, 1, 2, and 3
        uint8_t palette_color = (bgp >> (color_id * 2)) & 0x03;
        framebuffer[ly * 160 + px] = shades[palette_color];
    }
}

void PPU::request_interrupt(uint8_t bit) {
    uint8_t if_reg = mmu->read_byte(0xFF0F);
    if_reg |= (1 << bit);
    mmu->write_byte(0xFF0F, if_reg);
}