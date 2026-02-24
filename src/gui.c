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

void gui_draw_board(Game* game, SDL_Renderer* renderer, SDL_Texture* font_texture) {
    (void)game;
    (void)renderer;
    (void)font_texture;
}

void gui_handle_click(Game* game, int mouse_x, int mouse_y) {
    (void)game;
    (void)mouse_x;
    (void)mouse_y;
}

void gui_draw_game_over(Game* game, SDL_Renderer* renderer) {
    (void)game;
    (void)renderer;
}

#else

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 700

#define COLOR_BG_R 30
#define COLOR_BG_G 30
#define COLOR_BG_B 30

static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static TTF_Font* g_font = NULL;
static TTF_Font* g_big_font = NULL;
static GUIState* g_gui = NULL;

static SDL_Texture* render_text(TTF_Font* font, const char* text, SDL_Color color) {
    if (!font || !text || !g_renderer) {
        return NULL;
    }

    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) {
        return NULL;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(g_renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

static TTF_Font* try_open_font(int size) {
    static const char* font_paths[] = {
        "config/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "C:\\Windows\\Fonts\\arial.ttf"
    };

    for (size_t i = 0; i < sizeof(font_paths) / sizeof(font_paths[0]); i++) {
        TTF_Font* font = TTF_OpenFont(font_paths[i], size);
        if (font) {
            return font;
        }
    }

    return NULL;
}

static void sync_board_layout(const Game* game) {
    if (!g_gui || !game) {
        return;
    }

    int max_cell = (WINDOW_WIDTH - 80) / game->size;
    if (max_cell < 60) {
        max_cell = 60;
    }
    g_gui->cell_size = max_cell;

    const int board_pixels = g_gui->cell_size * game->size;
    g_gui->board_offset_x = (WINDOW_WIDTH - board_pixels) / 2;
    g_gui->board_offset_y = 150;
}

static void draw_board_lines(const Game* game) {
    if (!game || !g_renderer || !g_gui) {
        return;
    }

    SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);
    for (uint8_t i = 1; i < game->size; i++) {
        int x = g_gui->board_offset_x + i * g_gui->cell_size;
        SDL_Rect v_line = {x - 1, g_gui->board_offset_y, 2, game->size * g_gui->cell_size};
        SDL_RenderFillRect(g_renderer, &v_line);

        int y = g_gui->board_offset_y + i * g_gui->cell_size;
        SDL_Rect h_line = {g_gui->board_offset_x, y - 1, game->size * g_gui->cell_size, 2};
        SDL_RenderFillRect(g_renderer, &h_line);
    }
}

static void draw_cell(const Game* game, uint8_t row, uint8_t col) {
    if (!game || !g_renderer || !g_gui) {
        return;
    }

    Player p = game->board[row][col];
    if (p == PLAYER_NONE) {
        return;
    }

    int x = g_gui->board_offset_x + col * g_gui->cell_size + g_gui->cell_size / 2;
    int y = g_gui->board_offset_y + row * g_gui->cell_size + g_gui->cell_size / 2;
    int size = g_gui->cell_size / 3;

    if (p == PLAYER_X) {
        SDL_SetRenderDrawColor(g_renderer, 0, 255, 255, 255);
        SDL_RenderDrawLine(g_renderer, x - size, y - size, x + size, y + size);
        SDL_RenderDrawLine(g_renderer, x - size, y + size, x + size, y - size);
    } else if (p == PLAYER_O) {
        SDL_SetRenderDrawColor(g_renderer, 255, 0, 255, 255);
        for (int r = size; r > size - 4 && r > 0; r--) {
            SDL_Rect rect = {x - r, y - r, r * 2, r * 2};
            SDL_RenderDrawRect(g_renderer, &rect);
        }
    }
}

static void draw_highlight(const Game* game) {
    if (!game || !g_renderer || !g_gui) {
        return;
    }
    if (game->state != GAME_STATE_WIN || game->win_line[0] < 0) {
        return;
    }

    int x1 = g_gui->board_offset_x + game->win_line[1] * g_gui->cell_size + g_gui->cell_size / 2;
    int y1 = g_gui->board_offset_y + game->win_line[0] * g_gui->cell_size + g_gui->cell_size / 2;
    int x2 = g_gui->board_offset_x + game->win_line[3] * g_gui->cell_size + g_gui->cell_size / 2;
    int y2 = g_gui->board_offset_y + game->win_line[2] * g_gui->cell_size + g_gui->cell_size / 2;

    SDL_SetRenderDrawColor(g_renderer, 0, 255, 0, 255);
    SDL_RenderDrawLine(g_renderer, x1, y1, x2, y2);
}

bool gui_init(GUIState* gui) {
    if (!gui) {
        return false;
    }

    gui->initialized = false;
    gui->use_gui = false;
    gui->window_width = WINDOW_WIDTH;
    gui->window_height = WINDOW_HEIGHT;
    gui->cell_size = 120;
    gui->board_offset_x = (WINDOW_WIDTH - (gui->cell_size * 3)) / 2;
    gui->board_offset_y = 150;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return false;
    }

    if (TTF_Init() < 0) {
        fprintf(stderr, "SDL_ttf init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return false;
    }

    g_window = SDL_CreateWindow(
        "TicTacToe-CX",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        gui->window_width,
        gui->window_height,
        SDL_WINDOW_SHOWN
    );
    if (!g_window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    g_font = try_open_font(24);
    g_big_font = try_open_font(48);

    gui->initialized = true;
    gui->use_gui = true;
    g_gui = gui;
    return true;
}

void gui_close(GUIState* gui) {
    (void)gui;

    if (g_big_font) {
        TTF_CloseFont(g_big_font);
        g_big_font = NULL;
    }
    if (g_font) {
        TTF_CloseFont(g_font);
        g_font = NULL;
    }
    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }

    g_gui = NULL;
    TTF_Quit();
    SDL_Quit();
}

void gui_draw_board(Game* game, SDL_Renderer* renderer, SDL_Texture* font_texture) {
    (void)renderer;
    (void)font_texture;
    if (!game || !g_renderer || !g_gui) {
        return;
    }

    sync_board_layout(game);

    SDL_SetRenderDrawColor(g_renderer, COLOR_BG_R, COLOR_BG_G, COLOR_BG_B, 255);
    SDL_RenderClear(g_renderer);

    SDL_Texture* title = render_text(g_big_font, "TicTacToe-CX", (SDL_Color){0, 255, 255, 255});
    if (title) {
        int w = 0;
        int h = 0;
        SDL_QueryTexture(title, NULL, NULL, &w, &h);
        SDL_Rect dst = {(WINDOW_WIDTH - w) / 2, 30, w, h};
        SDL_RenderCopy(g_renderer, title, NULL, &dst);
        SDL_DestroyTexture(title);
    }

    if (g_font) {
        char turn_text[32];
        char symbol = (game->current_player == PLAYER_X) ? game->symbol_x : game->symbol_o;
        snprintf(turn_text, sizeof(turn_text), "%c's Turn", symbol);
        SDL_Color turn_color = (game->current_player == PLAYER_X)
            ? (SDL_Color){0, 255, 255, 255}
            : (SDL_Color){255, 0, 255, 255};
        SDL_Texture* turn = render_text(g_font, turn_text, turn_color);
        if (turn) {
            int w = 0;
            int h = 0;
            SDL_QueryTexture(turn, NULL, NULL, &w, &h);
            SDL_Rect dst = {(WINDOW_WIDTH - w) / 2, 100, w, h};
            SDL_RenderCopy(g_renderer, turn, NULL, &dst);
            SDL_DestroyTexture(turn);
        }
    }

    draw_board_lines(game);

    for (uint8_t i = 0; i < game->size; i++) {
        for (uint8_t j = 0; j < game->size; j++) {
            draw_cell(game, i, j);
        }
    }

    draw_highlight(game);
    SDL_RenderPresent(g_renderer);
}

void gui_handle_click(Game* game, int mouse_x, int mouse_y) {
    if (!game || !g_gui) {
        return;
    }
    if (mouse_x < g_gui->board_offset_x || mouse_y < g_gui->board_offset_y) {
        return;
    }

    int col = (mouse_x - g_gui->board_offset_x) / g_gui->cell_size;
    int row = (mouse_y - g_gui->board_offset_y) / g_gui->cell_size;

    if (row >= 0 && row < (int)game->size && col >= 0 && col < (int)game->size) {
        game_make_move(game, (uint8_t)row, (uint8_t)col);
    }
}

void gui_draw_game_over(Game* game, SDL_Renderer* renderer) {
    (void)renderer;
    if (!game || !g_renderer || !g_big_font || !g_font) {
        return;
    }

    const char* msg = "";
    SDL_Color color = {255, 255, 255, 255};

    if (game->state == GAME_STATE_WIN) {
        Player winner = game_get_winner(game);
        msg = (winner == PLAYER_X) ? "X WINS!" : "O WINS!";
        color = (SDL_Color){0, 255, 0, 255};
    } else if (game->state == GAME_STATE_DRAW) {
        msg = "DRAW!";
        color = (SDL_Color){255, 255, 0, 255};
    }

    SDL_Texture* over = render_text(g_big_font, msg, color);
    if (over) {
        int w = 0;
        int h = 0;
        SDL_QueryTexture(over, NULL, NULL, &w, &h);
        SDL_Rect dst = {(WINDOW_WIDTH - w) / 2, 520, w, h};
        SDL_RenderCopy(g_renderer, over, NULL, &dst);
        SDL_DestroyTexture(over);
    }

    SDL_Texture* hint = render_text(g_font, "SPACE: restart   ESC: quit", (SDL_Color){255, 255, 255, 255});
    if (hint) {
        int w = 0;
        int h = 0;
        SDL_QueryTexture(hint, NULL, NULL, &w, &h);
        SDL_Rect dst = {(WINDOW_WIDTH - w) / 2, 585, w, h};
        SDL_RenderCopy(g_renderer, hint, NULL, &dst);
        SDL_DestroyTexture(hint);
    }

    SDL_RenderPresent(g_renderer);
}

bool gui_run_game(Game* game) {
    if (!game || !g_window || !g_renderer) {
        return false;
    }

    SDL_Event e;
    bool quit = false;
    bool game_running = true;

    while (!quit && game_running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_MOUSEBUTTONDOWN && game->state == GAME_STATE_PLAYING) {
                gui_handle_click(game, e.button.x, e.button.y);
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    game_running = false;
                } else if (e.key.keysym.sym == SDLK_SPACE &&
                           (game->state == GAME_STATE_WIN || game->state == GAME_STATE_DRAW)) {
                    game_reset(game);
                }
            }
        }

        gui_draw_board(game, g_renderer, NULL);
        if (game->state == GAME_STATE_WIN || game->state == GAME_STATE_DRAW) {
            gui_draw_game_over(game, g_renderer);
        }

        SDL_Delay(16);
    }

    return !quit;
}

#endif
