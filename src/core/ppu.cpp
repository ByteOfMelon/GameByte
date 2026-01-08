#include "ppu.h"
#include <cstring>

PPU::PPU() {
    // Initialize registers to Post-Boot ROM defaults
    lcdc = 0x91; // LCD enabled, Window enabled, BG window/tile Data @ $8000
    stat = 0x85;
    scy = 0x00;
    scx = 0x00;
    lyc = 0x00;
    bgp = 0xFC;
    
    current_ly = 0;
    ppu_cycles = 0;
    mode = 2; // Default - OAM search

    // Clear framebuffer
    memset(framebuffer, 0, sizeof(framebuffer));
}

void PPU::connect_mmu(MMU* m) {
    mmu = m;
}

void PPU::init_sdl() {
    SDL_Init(SDL_INIT_VIDEO);

    // Initialize the window. Render at 2x scale for visibility
    window = SDL_CreateWindow("GameByte", (160*2), (144*2), 0);
    renderer = SDL_CreateRenderer(window, nullptr);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
}

void PPU::render_frame() {
    SDL_UpdateTexture(texture, NULL, framebuffer, 160 * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void PPU::render_blank() {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

void PPU::tick(uint8_t cycles) {
    // Check if LCD is enabled (LCDC bit 7)
    if (!(lcdc & 0x80)) {
        ppu_cycles = 0;
        current_ly = 0;
        mode = 2; // When LCD is re-enabled, restart in OAM Search (Mode 2)
        return;
    }

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
                    // Reset to start of next frame
                    current_ly = 0;
                    window_line_counter = 0;
                    mode = 2;
                }
            }
            break;
    }

    // Update the STAT register's bits 0-1
    stat &= ~0x03;
    stat |= (mode & 0x03);

    // Handle LYC == LY comparison (bit 2 of STAT)
    if (current_ly == lyc) {
        bool was_coincidence = (stat & 0x04);
        stat |= 0x04;
        
        if (!was_coincidence && (stat & 0x40)) {
            request_interrupt(1);
        }
    } else {
        stat &= ~0x04;
    }

    // Trigger STAT interrupt on mode changes
    if (mode != last_mode) {
        if (mode == 0 && (stat & 0x08)) request_interrupt(1);
        if (mode == 1 && (stat & 0x10)) request_interrupt(1);
        if (mode == 2 && (stat & 0x20)) request_interrupt(1);
        
        last_mode = mode; // Update last_mode for the next tick
    }
}

void PPU::draw_scanline() {
    // Get current scanline position
    uint8_t ly = current_ly;

    // Check if scanline is beyond visible area
    if (ly >= 144) return;
    
    uint8_t lcdc = mmu->read_byte(0xFF40);

    // Get the background palette (BGP) at 0xFF47
    uint8_t bgp = mmu->read_byte(0xFF47);
    uint32_t shades[] = { 0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000 };

    // Sprite priority check
    uint8_t bg_color_ids[160] = {0};

    // Check master bg/window enable bit (LCDC bit 0)
    if (!(lcdc & 0x01)) {
        // Fill scanline with white (color 0)
        for (int px = 0; px < 160; px++) {
            framebuffer[ly * 160 + px] = shades[0];
        }
        return;
    } else {
        // Window positions
        uint8_t wy = mmu->read_byte(0xFF4A);
        uint8_t wx = mmu->read_byte(0xFF4B) - 7;
        bool window_enabled = (lcdc & 0x20) && (ly >= wy);
        bool window_drawn = false;

        // Get scroll (x and y) pos
        uint8_t scy = mmu->read_byte(0xFF42);
        uint8_t scx = mmu->read_byte(0xFF43);

        for (int px = 0; px < 160; px++) {
            uint16_t map_base;
            uint8_t t_x, t_y;

            // Decide if window or background
            if (window_enabled && px >= wx) {
                map_base = (lcdc & 0x40) ? 0x9C00 : 0x9800; // LCDC bit 6
                t_x = px - wx;
                t_y = window_line_counter;
                window_drawn = true;
            } else {
                map_base = (lcdc & 0x08) ? 0x9C00 : 0x9800; // LCDC bit 3
                t_x = px + scx;
                t_y = ly + scy;
            }

            uint16_t tile_row = (t_y / 8) * 32;
            uint16_t tile_col = (t_x / 8);
            uint8_t tile_index = mmu->read_byte(map_base + tile_row + tile_col);

            // Tile data addressing
            bool is_unsigned = (lcdc & 0x10);
            uint16_t tile_data_addr;
            
            if (is_unsigned) {
                tile_data_addr = 0x8000 + (tile_index * 16);
            } else {
                int16_t rel_address = static_cast<int8_t>(tile_index) * 16;
                tile_data_addr = static_cast<uint16_t>(0x9000 + rel_address);
            }

            // Fetch pixel bits
            uint8_t line = (t_y % 8) * 2;
            uint8_t b1 = mmu->read_byte(tile_data_addr + line);
            uint8_t b2 = mmu->read_byte(tile_data_addr + line + 1);

            int bit = 7 - (t_x % 8);
            uint8_t color_id = ((b2 >> bit) & 0x01) << 1 | ((b1 >> bit) & 0x01);

            bg_color_ids[px] = color_id;

            // Apply palette and write to framebuffer
            uint8_t palette_color = (bgp >> (color_id * 2)) & 0x03;
            framebuffer[ly * 160 + px] = shades[palette_color];
        }

        if (window_drawn) {
            window_line_counter++;
        }
    }

    // Draw sprite(s) (10 max per line) on top of background
    if (lcdc & 0x02) {
        int sprites_on_line = 0;
        uint8_t used_sprite_x_coords[160] = {};
        for (int i = 0; i < 40; i++) {
            uint16_t oam_addr = 0xFE00 + (i * 4);
            if (sprites_on_line >= 10) break;
            
            // Get proper sprite attributes
            uint8_t sprite_y = mmu->read_byte(oam_addr) - 16;
            uint8_t sprite_x = mmu->read_byte(oam_addr + 1) - 8;
            uint8_t tile_index = mmu->read_byte(oam_addr + 2);
            uint8_t attributes = mmu->read_byte(oam_addr + 3);
            uint8_t sprite_height = (lcdc & 0x04) ? 16 : 8;

            // Check if sprite is visible on this scanline (ly)
            if (ly >= sprite_y && ly < sprite_y + sprite_height) {
                sprites_on_line++;

                // Check if sprite at this X coordinate has already been used (priority)
                if (sprite_x >= 0 && sprite_x < 160) {
                    if (used_sprite_x_coords[sprite_x]) {
                        continue;
                    } else {
                        used_sprite_x_coords[sprite_x] = 1;
                    }
                }

                // Determine which palette to use (Bit 4: 0=OBP0, 1=OBP1)
                uint8_t obp = mmu->read_byte((attributes & 0x10) ? 0xFF49 : 0xFF48);
                
                // Fetch tile data (Sprites always use 0x8000-0x8FFF unsigned mode)
                uint16_t tile_addr;
                uint8_t line = (ly - sprite_y);

                // Handle vertical flip (Bit 6)
                if (attributes & 0x40) line = (sprite_height - 1) - line;

                if (sprite_height == 16) {
                    // For 8x16 sprites, bit 0 of the tile index is ignored for the base
                    uint8_t base_tile = tile_index & 0xFE;
                    uint8_t actual_tile = (line < 8) ? base_tile : (base_tile | 0x01);
                    
                    tile_addr = 0x8000 + (actual_tile * 16);
                    // Reset line to 0-7 for the specific 8x8 tile selected
                    line %= 8;
                } else {
                    // Standard 8x8 mode
                    tile_addr = 0x8000 + (tile_index * 16);
                }

                uint8_t b1 = mmu->read_byte(tile_addr + (line * 2));
                uint8_t b2 = mmu->read_byte(tile_addr + (line * 2) + 1);

                for (int x = 0; x < 8; x++) {
                    int pixel_x = sprite_x + x;
                    if (pixel_x < 0 || pixel_x >= 160) continue;

                    // Handle horizontal flip (Bit 5)
                    int bit = (attributes & 0x20) ? x : 7 - x;
                    uint8_t color_id = ((b2 >> bit) & 0x01) << 1 | ((b1 >> bit) & 0x01);

                    // Don't draw transparent pixels (color 0)
                    if (color_id != 0) {
                        // OBJ-to-BG priority (OAM bit 7)
                        uint8_t bg_id = bg_color_ids[pixel_x];
                        bool bg_over_obj = (attributes & 0x80) != 0;

                        if (!bg_over_obj || (bg_over_obj && bg_id == 0)) {
                            uint8_t palette_color = (obp >> (color_id * 2)) & 0x03;
                            framebuffer[ly * 160 + pixel_x] = shades[palette_color];
                        }
                    }
                }
            }
        }
    }
}

void PPU::request_interrupt(uint8_t bit) {
    uint8_t if_reg = mmu->read_byte(0xFF0F);
    if_reg |= (1 << bit);
    mmu->write_byte(0xFF0F, if_reg);
}