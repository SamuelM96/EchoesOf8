cmake_minimum_required(VERSION 3.10)

project(eo8)

set(CMAKE_CXX_STANDARD 11)

if(APPLE)
    set(CMAKE_BUILD_RPATH /Library/Frameworks/)
endif()

add_executable(eo8  src/main.c
                    src/assembler.c
                    src/instructions.c
                    src/emulator.c
                    src/disassembler.c)

find_package(SDL2 REQUIRED COMPONENTS SDL2)
target_link_libraries(eo8 PRIVATE SDL2::SDL2)

include(GNUInstallDirs)
