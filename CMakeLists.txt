cmake_minimum_required(VERSION 3.10)

project(eo8)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB Eo8Sources src/*.c)
file(GLOB LibSources lib/*.c)
add_executable(eo8  ${Eo8Sources} ${LibSources})

find_package(SDL2 REQUIRED COMPONENTS SDL2)
target_link_libraries(eo8 PRIVATE SDL2::SDL2 m)
target_include_directories(eo8 PRIVATE include ${SDL2_INCLUDE_DIRS})

include(GNUInstallDirs)
