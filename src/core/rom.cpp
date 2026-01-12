#include "rom.h"
#include <cstdio>
#include <string>
const unsigned char* ROM::data = nullptr;
size_t ROM::size = 0;

void ROM::unload() {
    if (data) {
        delete[] data;
        data = nullptr;
        size = 0;
    }
}

bool ROM::load(const char* filename) {
    unload(); // Unload any existing ROM

    // Open file
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return false;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate memory and read file
    data = new unsigned char[size];
    fread((void*)data, 1, size, file);
    fclose(file);

    // Only allow supported ROM types
    switch (data[OFFSET_TYPE]) {
        case ROM_PLAIN:
            break;
        case ROM_MBC1:
        case ROM_MBC1_RAM:
        case ROM_MBC1_RAM_BATT:
            break;
        default:
            printf("[ROM] Unsupported or unimplemented ROM type: 0x%02X\n", data[OFFSET_TYPE]);
            unload();
            return false;
            break;
    }

    // Debug - log ROM's header values
    printf("[ROM] Successfully loaded ROM: %s\n", filename);
    std::string title(reinterpret_cast<const char*>(data + OFFSET_TITLE), 16);
    printf("[ROM] ROM title: %s\n", title.c_str());
    printf("[ROM] ROM size: %zu bytes\n", size);
    unsigned char rom_type = data[OFFSET_TYPE];
    printf("[ROM] ROM type: 0x%02X\n", rom_type);
    unsigned char rom_size = data[OFFSET_ROM_SIZE];
    printf("[ROM] ROM size byte: 0x%02X\n", rom_size);
    unsigned char ram_size = data[OFFSET_RAM_SIZE];
    printf("[ROM] RAM size byte: 0x%02X\n", ram_size);

    return true;
}