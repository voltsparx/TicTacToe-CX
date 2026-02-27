cmake_minimum_required(VERSION 3.10)
project(TicTacToe-CX C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(OUTPUTCONFIG IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/bin")
endforeach()

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

include_directories(${CMAKE_SOURCE_DIR}/src)

option(ENABLE_SDL2 "Enable SDL2 GUI support" ON)

find_package(OpenSSL REQUIRED)
set(SDL2_ENABLED FALSE)
set(SDL_LINK_LIBS)

set(SOURCES
    src/main.c
    src/game.c
    src/cli.c
    src/ai.c
    src/network.c
    src/internet.c
    src/utils.c
    src/achievements.c
    src/replay.c
    src/sound.c
    src/gui.c
        src/cli.c
        src/cli.c
)

if(ENABLE_SDL2)
    find_package(SDL2 QUIET)
    find_package(SDL2_ttf QUIET)

    if(SDL2_FOUND AND (SDL2_ttf_FOUND OR SDL2_TTF_FOUND))
        add_definitions(-DSDL2_AVAILABLE)
        set(SDL2_ENABLED TRUE)

        if(TARGET SDL2::SDL2)
            list(APPEND SDL_LINK_LIBS SDL2::SDL2)
        elseif(SDL2_LIBRARIES)
            include_directories(${SDL2_INCLUDE_DIRS})
            list(APPEND SDL_LINK_LIBS ${SDL2_LIBRARIES})
        endif()

        if(TARGET SDL2_ttf::SDL2_ttf)
            list(APPEND SDL_LINK_LIBS SDL2_ttf::SDL2_ttf)
        elseif(SDL2_TTF_LIBRARIES)
            include_directories(${SDL2_TTF_INCLUDE_DIRS})
            list(APPEND SDL_LINK_LIBS ${SDL2_TTF_LIBRARIES})
        endif()
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2_PKG QUIET IMPORTED_TARGET sdl2)
            pkg_check_modules(SDL2_TTF_PKG QUIET IMPORTED_TARGET SDL2_ttf)
            if(SDL2_PKG_FOUND AND SDL2_TTF_PKG_FOUND)
                add_definitions(-DSDL2_AVAILABLE)
                set(SDL2_ENABLED TRUE)
                list(APPEND SDL_LINK_LIBS PkgConfig::SDL2_PKG PkgConfig::SDL2_TTF_PKG)
            endif()
        endif()
    endif()

    if(SDL2_ENABLED)
        message(STATUS "SDL2 support: ENABLED")
    else()
        message(STATUS "SDL2 support: NOT FOUND (GUI disabled)")
    endif()
else()
    message(STATUS "SDL2 support: DISABLED")
endif()

add_executable(${PROJECT_NAME} ${SOURCES}
        src/cli.c)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "tictactoe-cx")

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W4>
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(${PROJECT_NAME} OpenSSL::Crypto)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32 winmm)
endif()

if(SDL2_ENABLED AND SDL_LINK_LIBS)
    target_link_libraries(${PROJECT_NAME} ${SDL_LINK_LIBS})
endif()

message(STATUS "")
message(STATUS "TicTacToe-CX Build Configuration")
message(STATUS "  Version: v3.1")
message(STATUS "  C Standard: C99")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Features: CLI, AI, Network, Achievements, Replay")
message(STATUS "  Crypto: OpenSSL")
