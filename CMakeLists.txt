cmake_minimum_required(VERSION 3.10) # Or a newer version like 3.15+ for SDL3
project(GameBoyEmulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17) # Or your preferred C++ standard (e.g., 14, 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_COMPILER "/usr/bin/clang++") # Explicitly set Clang on macOS if needed

# Find SDL3
find_package(SDL3 REQUIRED)
include_directories(${SDL3_INCLUDE_DIRS})

# If you installed SDL3 extension libraries (names might vary):
# find_package(SDL3_image REQUIRED)
# find_package(SDL3_mixer REQUIRED)
# find_package(SDL3_ttf REQUIRED)
#include_directories(${SDL3_IMAGE_INCLUDE_DIRS} ${SDL3_TTF_INCLUDE_DIRS})

# Add your source files here as you create them
add_executable(GameByte src/main.cpp
                          # src/core/cpu.cpp
                          # src/core/ppu.cpp
                          # src/core/mmu.cpp
                          # Add other.cpp files as you create them
                          )

# Link against SDL3
target_link_libraries(GameByte PRIVATE SDL3::SDL3) # Modern CMake target for SDL3

# If you installed SDL3 extension libraries (names might vary):
# target_link_libraries(GameByte PRIVATE SDL3::SDL3_image SDL3::SDL3_ttf)