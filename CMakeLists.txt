cmake_minimum_required(VERSION 3.16) # Or a newer version like 3.15+ for SDL3
project(GameByte LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
if(APPLE)
    # Use @rpath for dynamic libraries
    set(CMAKE_MACOSX_RPATH 1)
    # Search for libraries in the same folder as the executable
    set(CMAKE_INSTALL_RPATH "@executable_path/")
    # Apply these paths even during the build phase (important for the GitHub runner)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# prevent installing to system directories. 
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")

# Find SDL3
if(WIN32)
    # Build SDL3 from source on Windows (vendored)
    add_subdirectory(vendored/SDL)
else()
    # On macOS/Linux types, look for system SDL3
    find_package(SDL3 REQUIRED)
    # include_directories(${SDL3_INCLUDE_DIRS}) # Not strictly needed if using target_link_libraries with SDL3::SDL3
endif()

# If you installed SDL3 extension libraries (names might vary):
# find_package(SDL3_image REQUIRED)
# find_package(SDL3_mixer REQUIRED)
# find_package(SDL3_ttf REQUIRED)
#include_directories(${SDL3_IMAGE_INCLUDE_DIRS} ${SDL3_TTF_INCLUDE_DIRS})

# Add your source files here as you create them
add_executable(GameByte src/main.cpp
                          src/core/cpu.cpp
                          src/core/mmu.cpp
                          src/core/rom.cpp
                          src/core/ppu.cpp
                          # Add other.cpp files as you create them
                          )

# Link against SDL3
if(WIN32)
    target_link_libraries(GameByte PRIVATE SDL3::SDL3)
else()
    target_link_libraries(GameByte PRIVATE SDL3::SDL3)
endif()

# If you installed SDL3 extension libraries (names might vary):
# target_link_libraries(GameByte PRIVATE SDL3::SDL3_image SDL3::SDL3_ttf)