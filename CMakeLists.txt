cmake_minimum_required(VERSION 3.10)

project(eo8)

set(CMAKE_CXX_STANDARD 11)

find_package(SDL2 REQUIRED)

add_executable(eo8  src/main.c
                    src/assembler.c
                    src/instructions.c
                    src/emulator.c
                    src/disassembler.c)

target_include_directories(eo8 PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(eo8 ${SDL2_LIBRARIES})

include(GNUInstallDirs)
