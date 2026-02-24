cmake_minimum_required(VERSION 3.10)
project(TicTacToe-CX C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

include_directories(${CMAKE_SOURCE_DIR}/src)

option(ENABLE_SDL2 "Enable SDL2 GUI support" ON)

find_package(OpenSSL REQUIRED)

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
    src/gui.c
)

if(ENABLE_SDL2)
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        add_definitions(-DSDL2_AVAILABLE)
        include_directories(${SDL2_INCLUDE_DIRS})
        message(STATUS "SDL2 support: ENABLED")
    else()
        message(STATUS "SDL2 support: NOT FOUND (GUI disabled)")
    endif()
else()
    message(STATUS "SDL2 support: DISABLED")
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "tictactoe-cx")

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W4>
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32 winmm OpenSSL::Crypto)
    if(SDL2_FOUND)
        target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
    endif()
else()
    target_link_libraries(${PROJECT_NAME} OpenSSL::Crypto)
endif()

message(STATUS "")
message(STATUS "TicTacToe-CX Build Configuration")
message(STATUS "  Version: 1.0")
message(STATUS "  C Standard: C99")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Features: CLI, AI, Network, Achievements, Replay")
message(STATUS "  Crypto: OpenSSL")
