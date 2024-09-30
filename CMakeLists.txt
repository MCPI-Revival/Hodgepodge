cmake_minimum_required(VERSION 3.16.0)

# Build For ARM
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_SYSTEM_NAME "Linux")
set(CMAKE_SYSTEM_PROCESSOR "arm")

# Start Project
project(hodgepodge)

# Include SDK
include("$ENV{HOME}/.minecraft-pi/sdk/lib/minecraft-pi-reborn/sdk/sdk.cmake")

# Build
add_library(redstone SHARED
    src/conveyor.cpp
    src/nether_wand.cpp
    src/tiny.cpp
    src/oddly_bright_block.cpp
    src/camera.cpp
    src/commands.cpp
    src/ender_pearls.cpp
    src/bomb.cpp
    src/init.cpp
    src/block_frame.cpp
    src/animal-following.cpp
    src/rendering.cpp
    src/redstone.cpp
    src/api.cpp
    src/dash.cpp
    src/jungle_tree.cpp
    src/pedestal.cpp
    src/pistons.cpp
)
target_link_libraries(redstone mods reborn-patch symbols)
