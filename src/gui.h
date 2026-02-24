#ifndef GUI_H
#define GUI_H

#include "game.h"
#include <stdbool.h>

#ifdef SDL2_AVAILABLE
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_ttf.h>
#else
    typedef struct SDL_Renderer SDL_Renderer;
    typedef struct SDL_Texture SDL_Texture;
#endif

typedef struct {
    bool initialized;
    bool use_gui;
    int window_width;
    int window_height;
    int cell_size;
    int board_offset_x;
    int board_offset_y;
} GUIState;

bool gui_init(GUIState* gui);
void gui_close(GUIState* gui);
bool gui_run_game(Game* game);
void gui_draw_board(Game* game, SDL_Renderer* renderer, SDL_Texture* font_texture);
void gui_handle_click(Game* game, int mouse_x, int mouse_y);
void gui_draw_game_over(Game* game, SDL_Renderer* renderer);

#endif
