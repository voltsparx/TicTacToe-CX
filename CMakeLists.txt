cmake_minimum_required(VERSION 3.10)
project(TicTacToe-CX C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

include_directories(${CMAKE_SOURCE_DIR}/src)

option(ENABLE_SDL2 "Enable SDL2 GUI support" ON)

set(SOURCES
    src/main.c
    src/game.c
    src/cli.c
    src/ai.c
    src/network.c
    src/utils.c
    src/achievements.c
    src/replay.c
    src/sound.c
)

if(ENABLE_SDL2)
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        add_definitions(-DSDL2_AVAILABLE)
        include_directories(${SDL2_INCLUDE_DIRS})
        list(APPEND SOURCES src/gui.c)
        message(STATUS "SDL2 support: ENABLED")
    else()
        message(STATUS "SDL2 support: NOT FOUND (GUI disabled)")
    endif()
else()
    message(STATUS "SDL2 support: DISABLED")
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W4>
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32 winmm)
    if(SDL2_FOUND)
        target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
    endif()
endif()

message(STATUS "")
message(STATUS "TicTacToe-CX Build Configuration")
message(STATUS "  Version: 1.0")
message(STATUS "  C Standard: C99")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Features: CLI, AI, Network, Achievements, Replay")
