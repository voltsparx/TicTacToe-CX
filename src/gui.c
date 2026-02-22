#include "gui.h"

#ifndef SDL2_AVAILABLE

bool gui_init(GUIState* gui) {
    if (!gui) return false;
    gui->initialized = false;
    gui->use_gui = false;
    return false;
}

void gui_close(GUIState* gui) {
    (void)gui;
}

bool gui_run_game(Game* game) {
    (void)game;
    return false;
}

#else

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>

#define WHITE SDL_Color{255, 255, 255, 255}
#define BLACK SDL_Color{0, 0, 0, 255}
#define CYAN SDL_Color{0, 255, 255, 255}
#define MAGENTA SDL_Color{255, 0, 255, 255}
#define GREEN SDL_Color{0, 255, 0, 255}
#define YELLOW SDL_Color{255, 255, 0, 255}
#define GRAY SDL_Color{100, 100, 100, 255}
#define DARK_GRAY SDL_Color{50, 50, 50, 255}

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static TTF_Font* font = NULL;
static TTF_Font* big_font = NULL;

static SDL_Texture* render_text(TTF_Font* font, const char* text, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return NULL;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

bool gui_init(GUIState* gui) {
    if (!gui) return false;
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        gui->initialized = false;
        gui->use_gui = false;
        return false;
    }
    
    if (TTF_Init() < 0) {
        printf("TTF could not initialize! TTF_Error: %s\n", TTF_GetError());
        SDL_Quit();
        gui->initialized = false;
        gui->use_gui = false;
        return false;
    }
    
    gui->window_width = 600;
    gui->window_height = 700;
    
    window = SDL_CreateWindow(
        "TicTacToe-CX",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        gui->window_width,
        gui->window_height,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        gui->initialized = false;
        gui->use_gui = false;
        return false;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        gui->initialized = false;
        gui->use_gui = false;
        return false;
    }
    
    font = TTF_OpenFont("config/arial.ttf", 24);
    big_font = TTF_OpenFont("config/arial.ttf", 48);
    
    if (!font || !big_font) {
        font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 24);
        big_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 48);
    }
    
    if (!font) font = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", 24);
    if (!big_font) big_font = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", 48);
    
    gui->cell_size = 120;
    gui->board_offset_x = (gui->window_width - (gui->cell_size * 3)) / 2;
    gui->board_offset_y = 150;
    
    gui->initialized = true;
    gui->use_gui = true;
    
    return true;
}

void gui_close(GUIState* gui) {
    (void)gui;
    
    if (big_font) {
        TTF_CloseFont(big_font);
        big_font = NULL;
    }
    if (font) {
        TTF_CloseFont(font);
        font = NULL;
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    
    TTF_Quit();
    SDL_Quit();
}

static void draw_board_lines(Game* game) {
    uint8_t n = game->size;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    
    for (uint8_t i = 1; i < n; i++) {
        int x = gui->board_offset_x + i * gui->cell_size;
        SDL_Rect v_line = {x - 2, gui->board_offset_y, 4, n * gui->cell_size};
        SDL_RenderFillRect(renderer, &v_line);
        
        int y = gui->board_offset_y + i * gui->cell_size;
        SDL_Rect h_line = {gui->board_offset_x, y - 2, n * gui->cell_size, 4};
        SDL_RenderFillRect(renderer, &h_line);
    }
}

static void draw_cell(Game* game, uint8_t row, uint8_t col) {
    Player p = game->board[row][col];
    if (p == PLAYER_NONE) return;
    
    int x = gui->board_offset_x + col * gui->cell_size + gui->cell_size / 2;
    int y = gui->board_offset_y + row * gui->cell_size + gui->cell_size / 2;
    int size = gui->cell_size / 3;
    
    if (p == PLAYER_X) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
        
        SDL_RenderDrawLine(renderer, x - size, y - size, x + size, y + size);
        SDL_RenderDrawLine(renderer, x - size, y + size, x + size, y - size);
    } else if (p == PLAYER_O) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        
        for (int r = size - 10; r > 0; r -= 2) {
            SDL_Rect rect = {x - r, y - r, r * 2, r * 2};
            SDL_RenderDrawRect(renderer, &rect);
        }
    }
}

static void draw_highlight(Game* game) {
    if (game->state != GAME_STATE_WIN || game->win_line[0] == -1) return;
    
    int x1 = gui->board_offset_x + game->win_line[1] * gui->cell_size + gui->cell_size / 2;
    int y1 = gui->board_offset_y + game->win_line[0] * gui->cell_size + gui->cell_size / 2;
    int x2 = gui->board_offset_x + game->win_line[3] * gui->cell_size + gui->cell_size / 2;
    int y2 = gui->board_offset_y + game->win_line[2] * gui->cell_size + gui->cell_size / 2;
    
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void gui_draw_board(Game* game, SDL_Renderer* renderer, SDL_Texture* font_texture) {
    (void)font_texture;
    
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);
    
    SDL_Texture* title = render_text(big_font, "TicTacToe-CX", CYAN);
    if (title) {
        int w, h;
        SDL_QueryTexture(title, NULL, NULL, &w, &h);
        SDL_Rect dst = {(600 - w) / 2, 30, w, h};
        SDL_RenderCopy(renderer, title, NULL, &dst);
        SDL_DestroyTexture(title);
    }
    
    char turn_text[32];
    Player current = game->current_player;
    snprintf(turn_text, sizeof(turn_text), "%c's Turn", current == PLAYER_X ? 'X' : 'O');
    SDL_Color turn_color = current == PLAYER_X ? CYAN : MAGENTA;
    SDL_Texture* turn_tex = render_text(font, turn_text, turn_color);
    if (turn_tex) {
        int w, h;
        SDL_QueryTexture(turn_tex, NULL, NULL, &w, &h);
        SDL_Rect dst = {(600 - w) / 2, 100, w, h};
        SDL_RenderCopy(renderer, turn_tex, NULL, &dst);
        SDL_DestroyTexture(turn_tex);
    }
    
    draw_board_lines(game);
    
    for (uint8_t i = 0; i < game->size; i++) {
        for (uint8_t j = 0; j < game->size; j++) {
            draw_cell(game, i, j);
        }
    }
    
    draw_highlight(game);
    
    SDL_RenderPresent(renderer);
}

void gui_handle_click(Game* game, int mouse_x, int mouse_y) {
    if (mouse_x < gui->board_offset_x || mouse_y < gui->board_offset_y) {
        return;
    }
    
    int col = (mouse_x - gui->board_offset_x) / gui->cell_size;
    int row = (mouse_y - gui->board_offset_y) / gui->cell_size;
    
    if (row >= 0 && row < (int)game->size && col >= 0 && col < (int)game->size) {
        game_make_move(game, (uint8_t)row, (uint8_t)col);
    }
}

void gui_draw_game_over(Game* game, SDL_Renderer* renderer) {
    (void)renderer;
    
    const char* msg = "";
    SDL_Color color = WHITE;
    
    if (game->state == GAME_STATE_WIN) {
        Player winner = game->current_player == PLAYER_X ? PLAYER_O : PLAYER_X;
        msg = winner == PLAYER_X ?!" : "O "X WINS WINS!";
        color = GREEN;
    } else if (game->state == GAME_STATE_DRAW) {
        msg = "DRAW!";
        color = YELLOW;
    }
    
    SDL_Texture* over_tex = render_text(big_font, msg, color);
    if (over_tex) {
        int w, h;
        SDL_QueryTexture(over_tex, NULL, NULL, &w, &h);
        SDL_Rect dst = {(600 - w) / 2, 500, w, h};
        SDL_RenderCopy(renderer, over_tex, NULL, &dst);
        SDL_DestroyTexture(over_tex);
    }
    
    SDL_Texture* restart_tex = render_text(font, "Press SPACE to restart, ESC to quit", WHITE);
    if (restart_tex) {
        int w, h;
        SDL_QueryTexture(restart_tex, NULL, NULL, &w, &h);
        SDL_Rect dst = {(600 - w) / 2, 580, w, h};
        SDL_RenderCopy(renderer, restart_tex, NULL, &dst);
        SDL_DestroyTexture(restart_tex);
    }
    
    SDL_RenderPresent(renderer);
}

bool gui_run_game(Game* game) {
    if (!game || !window || !renderer) return false;
    
    SDL_Event e;
    bool quit = false;
    bool game_running = true;
    
    while (!quit && game_running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (game->state == GAME_STATE_PLAYING) {
                    gui_handle_click(game, e.button.x, e.button.y);
                }
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    game_running = false;
                } else if (e.key.keysym.sym == SDLK_SPACE && 
                          (game->state == GAME_STATE_WIN || game->state == GAME_STATE_DRAW)) {
                    game_reset(game);
                }
            }
        }
        
        gui_draw_board(game, renderer, NULL);
        
        if (game->state == GAME_STATE_WIN || game->state == GAME_STATE_DRAW) {
            gui_draw_game_over(game, renderer);
        }
        
        SDL_Delay(16);
    }
    
    return !quit;
}

#endif
