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

bool gui_run_app(Config* cfg, Score* score, Sound* sound) {
    (void)cfg;
    (void)score;
    (void)sound;
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

#include "app_meta.h"
#include "ai.h"
#include "network.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 740

#define PANEL_X 80
#define PANEL_Y 120
#define PANEL_W (WINDOW_WIDTH - 160)
#define PANEL_H (WINDOW_HEIGHT - 180)

#define BTN_W (PANEL_W - 120)
#define BTN_H 50
#define BTN_GAP 12

typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_AI,
    SCREEN_NETWORK,
    SCREEN_NET_HOST,
    SCREEN_NET_JOIN,
    SCREEN_NET_WAIT,
    SCREEN_SETTINGS,
    SCREEN_SCORES,
    SCREEN_ABOUT,
    SCREEN_GAME
} GuiScreen;

typedef struct {
    SDL_Color bg;
    SDL_Color panel;
    SDL_Color border;
    SDL_Color text;
    SDL_Color muted;
    SDL_Color accent;
    SDL_Color btn;
    SDL_Color btn_selected;
    SDL_Color x_color;
    SDL_Color o_color;
    SDL_Color win_color;
    SDL_Color err_color;
} GuiTheme;

typedef struct {
    SDL_Rect rect;
    const char* label;
} Button;

typedef struct {
    int x;
    int y;
    int size;
    int cell;
} BoardLayout;

typedef struct {
    Config* cfg;
    Score* score;
    Sound* sound;

    GuiScreen screen;

    int sel_main;
    int sel_ai;
    int sel_net;
    int sel_settings;

    Game game;
    bool game_active;
    bool game_result_done;
    bool game_network;

    uint32_t ai_tick;
    uint32_t timer_tick;

    Network net;
    bool net_init;
    bool net_waiting;
    uint32_t net_wait_since;

    char host_port[8];
    char join_ip[16];
    char join_port[8];
    char passphrase[NETWORK_PASSPHRASE_MAX];

    int active_field;
    SDL_Rect field_rects[3];
    int field_count;
    bool text_input;

    char status[180];
    SDL_Color status_color;
    uint32_t status_until;

    bool running;
} App;

static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static TTF_Font* g_font = NULL;
static TTF_Font* g_font_small = NULL;
static TTF_Font* g_font_big = NULL;
static GUIState* g_gui = NULL;
static App g_app;

static const char* const MAIN_OPTIONS[] = {
    "Play vs AI",
    "Two Player Local",
    "LAN Multiplayer",
    "High Scores",
    "Settings",
    "About",
    "Quit Game"
};

static const char* const AI_OPTIONS[] = {"Easy AI", "Medium AI", "Hard AI", "Back"};
static const char* const NET_OPTIONS[] = {"Host Game", "Join Game", "Back"};

static int clamp_i(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static bool in_rect(int x, int y, const SDL_Rect* r) {
    return r && x >= r->x && x < (r->x + r->w) && y >= r->y && y < (r->y + r->h);
}

static void set_theme(const Config* cfg, GuiTheme* t) {
    int theme = cfg ? cfg->color_theme : 0;

    if (theme == 1) {
        *t = (GuiTheme){
            {18, 20, 30, 255}, {30, 35, 50, 240}, {78, 88, 126, 255},
            {238, 244, 255, 255}, {165, 180, 210, 255}, {70, 190, 255, 255},
            {46, 54, 78, 255}, {70, 190, 255, 255},
            {86, 222, 255, 255}, {255, 120, 210, 255}, {120, 255, 170, 255}, {255, 104, 104, 255}
        };
    } else if (theme == 2) {
        *t = (GuiTheme){
            {236, 242, 255, 255}, {255, 255, 255, 240}, {180, 194, 221, 255},
            {30, 45, 72, 255}, {98, 112, 145, 255}, {38, 124, 255, 255},
            {228, 236, 247, 255}, {38, 124, 255, 255},
            {20, 148, 255, 255}, {255, 80, 132, 255}, {0, 170, 112, 255}, {224, 68, 68, 255}
        };
    } else if (theme == 3) {
        *t = (GuiTheme){
            {22, 30, 14, 255}, {34, 52, 24, 240}, {98, 146, 54, 255},
            {208, 244, 146, 255}, {168, 214, 116, 255}, {174, 246, 84, 255},
            {42, 65, 26, 255}, {174, 246, 84, 255},
            {170, 255, 104, 255}, {255, 204, 95, 255}, {196, 255, 112, 255}, {255, 100, 100, 255}
        };
    } else {
        *t = (GuiTheme){
            {24, 28, 40, 255}, {38, 45, 62, 240}, {92, 108, 142, 255},
            {240, 246, 255, 255}, {176, 190, 216, 255}, {0, 198, 255, 255},
            {58, 68, 92, 255}, {0, 198, 255, 255},
            {70, 224, 255, 255}, {255, 96, 210, 255}, {124, 255, 170, 255}, {255, 112, 112, 255}
        };
    }
}

static TTF_Font* try_open_font(int size) {
    static const char* paths[] = {
        "config/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf"
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        TTF_Font* f = TTF_OpenFont(paths[i], size);
        if (f) return f;
    }
    return NULL;
}

static void draw_text_line(TTF_Font* font, const char* text, int x, int y, SDL_Color c, bool center) {
    if (!font || !text || !g_renderer) return;

    SDL_Surface* s = TTF_RenderUTF8_Blended(font, text, c);
    if (!s) return;

    SDL_Texture* t = SDL_CreateTextureFromSurface(g_renderer, s);
    if (!t) {
        SDL_FreeSurface(s);
        return;
    }

    SDL_Rect d;
    d.w = s->w;
    d.h = s->h;
    d.x = center ? (x - s->w / 2) : x;
    d.y = y;

    SDL_RenderCopy(g_renderer, t, NULL, &d);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}

static void draw_base(const GuiTheme* th, const char* subtitle) {
    SDL_SetRenderDrawColor(g_renderer, th->bg.r, th->bg.g, th->bg.b, 255);
    SDL_RenderClear(g_renderer);

    SDL_Rect panel = {PANEL_X, PANEL_Y, PANEL_W, PANEL_H};
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, th->panel.r, th->panel.g, th->panel.b, th->panel.a);
    SDL_RenderFillRect(g_renderer, &panel);
    SDL_SetRenderDrawColor(g_renderer, th->border.r, th->border.g, th->border.b, 255);
    SDL_RenderDrawRect(g_renderer, &panel);

    char title[96];
    (void)snprintf(title, sizeof(title), "%s v%s", APP_NAME, APP_VERSION);
    draw_text_line(g_font_big ? g_font_big : g_font, title, WINDOW_WIDTH / 2, 24, th->accent, true);
    if (subtitle && subtitle[0] != '\0') {
        draw_text_line(g_font, subtitle, WINDOW_WIDTH / 2, 82, th->muted, true);
    }
}

static void layout_buttons(const char* const* labels, int count, int top, Button* out) {
    int x = (WINDOW_WIDTH - BTN_W) / 2;
    for (int i = 0; i < count; i++) {
        out[i].rect = (SDL_Rect){x, top + i * (BTN_H + BTN_GAP), BTN_W, BTN_H};
        out[i].label = labels[i];
    }
}

static void draw_button(const GuiTheme* th, const Button* b, bool selected) {
    SDL_Color bg = selected ? th->btn_selected : th->btn;
    SDL_SetRenderDrawColor(g_renderer, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(g_renderer, &b->rect);
    SDL_SetRenderDrawColor(g_renderer, th->border.r, th->border.g, th->border.b, 255);
    SDL_RenderDrawRect(g_renderer, &b->rect);
    draw_text_line(g_font, b->label, b->rect.x + b->rect.w / 2, b->rect.y + 12, th->text, true);
}

static void set_status(App* a, const char* msg, SDL_Color c, uint32_t ms) {
    if (!a || !msg) return;
    (void)snprintf(a->status, sizeof(a->status), "%s", msg);
    a->status_color = c;
    a->status_until = SDL_GetTicks() + ms;
}

static void draw_status(App* a) {
    if (!a || a->status[0] == '\0') return;
    uint32_t now = SDL_GetTicks();
    if (now > a->status_until) {
        a->status[0] = '\0';
        return;
    }

    SDL_Rect box = {PANEL_X + 20, WINDOW_HEIGHT - 50, PANEL_W - 40, 30};
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 10, 12, 18, 190);
    SDL_RenderFillRect(g_renderer, &box);
    draw_text_line(g_font_small ? g_font_small : g_font, a->status, WINDOW_WIDTH / 2, WINDOW_HEIGHT - 45, a->status_color, true);
}

static void close_network(App* a) {
    if (!a) return;
    if (a->net_init) {
        network_close(&a->net);
    }
    a->net_init = false;
    a->net_waiting = false;
    a->game_network = false;
}

static void apply_cfg_to_game(Game* g, const Config* cfg) {
    if (!g || !cfg) return;

    if (cfg->timer_enabled && cfg->timer_seconds > 0) {
        game_start_timer(g, cfg->timer_seconds);
    } else {
        game_start_timer(g, 0);
    }

    g->player_symbol = (cfg->player_symbol == 'O') ? PLAYER_O : PLAYER_X;
}

static void start_game(App* a, GameMode mode) {
    if (!a || !a->cfg) return;

    game_init(&a->game, (uint8_t)a->cfg->board_size, mode);
    apply_cfg_to_game(&a->game, a->cfg);

    a->game_active = true;
    a->game_result_done = false;
    a->game_network = false;
    a->ai_tick = SDL_GetTicks() + 350;
    a->timer_tick = SDL_GetTicks();
    a->screen = SCREEN_GAME;
}

static void start_network_game(App* a, bool host) {
    if (!a || !a->cfg) return;

    game_init(&a->game, (uint8_t)a->cfg->board_size, host ? MODE_NETWORK_HOST : MODE_NETWORK_CLIENT);
    apply_cfg_to_game(&a->game, a->cfg);
    a->game.player_symbol = host ? PLAYER_X : PLAYER_O;

    a->game_active = true;
    a->game_result_done = false;
    a->game_network = true;
    a->timer_tick = SDL_GetTicks();
    a->screen = SCREEN_GAME;

    set_status(a, "Secure LAN session established", (SDL_Color){120, 255, 170, 255}, 2400);
}

static void save_scores_if_done(App* a) {
    if (!a || !a->score || a->game_result_done) return;
    if (a->game.state != GAME_STATE_WIN && a->game.state != GAME_STATE_DRAW) return;

    if (a->game.state == GAME_STATE_DRAW) {
        score_update(a->score, 0);
        if (a->sound) sound_play(a->sound, SOUND_DRAW);
    } else {
        Player winner = game_get_winner(&a->game);
        if (a->game.mode == MODE_LOCAL_2P) {
            score_update(a->score, (winner == PLAYER_X) ? 1 : -1);
            if (a->sound) sound_play(a->sound, SOUND_WIN);
        } else {
            bool player_won = (winner == a->game.player_symbol);
            score_update(a->score, player_won ? 1 : -1);
            if (a->sound) sound_play(a->sound, player_won ? SOUND_WIN : SOUND_LOSE);
        }
    }

    (void)score_save(a->score, get_highscore_path());
    a->game_result_done = true;
}

static int parse_port(const char* value) {
    int p = atoi(value ? value : "");
    if (p <= 0 || p > 65535) return DEFAULT_PORT;
    return p;
}

static void normalize_port(char* field, size_t size) {
    int p = parse_port(field);
    (void)snprintf(field, size, "%d", p);
}

static BoardLayout board_layout(const Game* g) {
    BoardLayout b = {0, 0, 0, 0};
    if (!g || g->size == 0) return b;

    int px = PANEL_H - 150;
    if (px > 500) px = 500;
    if (px < 240) px = 240;

    int cell = clamp_i(px / g->size, 50, 160);
    px = cell * g->size;

    b.size = px;
    b.cell = cell;
    b.x = (WINDOW_WIDTH - px) / 2;
    b.y = 170;
    return b;
}

static bool cell_from_mouse(const Game* g, int mx, int my, uint8_t* row, uint8_t* col) {
    if (!g || !row || !col) return false;

    BoardLayout b = board_layout(g);
    if (mx < b.x || my < b.y || mx >= b.x + b.size || my >= b.y + b.size) return false;

    int c = (mx - b.x) / b.cell;
    int r = (my - b.y) / b.cell;
    if (r < 0 || c < 0 || r >= g->size || c >= g->size) return false;

    *row = (uint8_t)r;
    *col = (uint8_t)c;
    return true;
}

static void draw_x(const GuiTheme* th, int cx, int cy, int r) {
    SDL_SetRenderDrawColor(g_renderer, th->x_color.r, th->x_color.g, th->x_color.b, 255);
    for (int o = -2; o <= 2; o++) {
        SDL_RenderDrawLine(g_renderer, cx - r, cy - r + o, cx + r, cy + r + o);
        SDL_RenderDrawLine(g_renderer, cx - r, cy + r + o, cx + r, cy - r + o);
    }
}

static void draw_o(const GuiTheme* th, int cx, int cy, int r) {
    SDL_SetRenderDrawColor(g_renderer, th->o_color.r, th->o_color.g, th->o_color.b, 255);
    for (int w = r; w > r - 4 && w > 4; w--) {
        SDL_Rect rect = {cx - w, cy - w, w * 2, w * 2};
        SDL_RenderDrawRect(g_renderer, &rect);
    }
}

static void draw_board(const GuiTheme* th, const Game* g) {
    BoardLayout b = board_layout(g);

    SDL_Rect br = {b.x, b.y, b.size, b.size};
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 12, 14, 22, 170);
    SDL_RenderFillRect(g_renderer, &br);

    SDL_SetRenderDrawColor(g_renderer, th->border.r, th->border.g, th->border.b, 255);
    for (uint8_t i = 1; i < g->size; i++) {
        int x = b.x + i * b.cell;
        SDL_Rect v = {x - 1, b.y, 2, b.size};
        SDL_RenderFillRect(g_renderer, &v);

        int y = b.y + i * b.cell;
        SDL_Rect h = {b.x, y - 1, b.size, 2};
        SDL_RenderFillRect(g_renderer, &h);
    }

    for (uint8_t r = 0; r < g->size; r++) {
        for (uint8_t c = 0; c < g->size; c++) {
            if (g->board[r][c] == PLAYER_NONE) continue;

            int cx = b.x + c * b.cell + b.cell / 2;
            int cy = b.y + r * b.cell + b.cell / 2;
            int rr = b.cell / 3;

            if (g->board[r][c] == PLAYER_X) draw_x(th, cx, cy, rr);
            else draw_o(th, cx, cy, rr);
        }
    }

    if (g->state == GAME_STATE_WIN && g->win_line[0] >= 0) {
        int x1 = b.x + g->win_line[1] * b.cell + b.cell / 2;
        int y1 = b.y + g->win_line[0] * b.cell + b.cell / 2;
        int x2 = b.x + g->win_line[3] * b.cell + b.cell / 2;
        int y2 = b.y + g->win_line[2] * b.cell + b.cell / 2;
        SDL_SetRenderDrawColor(g_renderer, th->win_color.r, th->win_color.g, th->win_color.b, 255);
        for (int o = -2; o <= 2; o++) SDL_RenderDrawLine(g_renderer, x1, y1 + o, x2, y2 + o);
    }
}

static void draw_main(const GuiTheme* th) {
    Button b[7];
    layout_buttons(MAIN_OPTIONS, 7, 185, b);
    draw_base(th, "GUI Main Menu");
    for (int i = 0; i < 7; i++) draw_button(th, &b[i], i == g_app.sel_main);
}

static void draw_ai(const GuiTheme* th) {
    Button b[4];
    layout_buttons(AI_OPTIONS, 4, 230, b);
    draw_base(th, "Select AI Difficulty");
    for (int i = 0; i < 4; i++) draw_button(th, &b[i], i == g_app.sel_ai);
}

static void draw_network(const GuiTheme* th) {
    Button b[3];
    layout_buttons(NET_OPTIONS, 3, 250, b);
    draw_base(th, "LAN Multiplayer");
    for (int i = 0; i < 3; i++) draw_button(th, &b[i], i == g_app.sel_net);
}

static void draw_waiting_host(const GuiTheme* th) {
    draw_base(th, "Hosting LAN Match");
    draw_text_line(g_font, "Waiting for another player...", WINDOW_WIDTH / 2, PANEL_Y + 220, th->text, true);
    draw_text_line(g_font_small ? g_font_small : g_font, "Esc: cancel", WINDOW_WIDTH / 2, WINDOW_HEIGHT - 84, th->muted, true);
}

static void draw_field(const GuiTheme* th, const char* label, const char* value, const SDL_Rect* rect, bool active, bool mask) {
    SDL_Color border = active ? th->accent : th->border;
    SDL_SetRenderDrawColor(g_renderer, 18, 22, 32, 220);
    SDL_RenderFillRect(g_renderer, rect);
    SDL_SetRenderDrawColor(g_renderer, border.r, border.g, border.b, 255);
    SDL_RenderDrawRect(g_renderer, rect);

    draw_text_line(g_font_small ? g_font_small : g_font, label, rect->x, rect->y - 24, th->muted, false);

    if (!mask) {
        draw_text_line(g_font, value, rect->x + 12, rect->y + 12, th->text, false);
    } else {
        char hidden[NETWORK_PASSPHRASE_MAX];
        size_t n = strlen(value);
        if (n >= sizeof(hidden)) n = sizeof(hidden) - 1;
        for (size_t i = 0; i < n; i++) hidden[i] = '*';
        hidden[n] = '\0';
        draw_text_line(g_font, hidden, rect->x + 12, rect->y + 12, th->text, false);
    }
}

static void draw_host_form(const GuiTheme* th) {
    draw_base(th, "Host LAN Game");

    g_app.field_count = 2;
    g_app.field_rects[0] = (SDL_Rect){PANEL_X + 90, PANEL_Y + 130, PANEL_W - 180, 52};
    g_app.field_rects[1] = (SDL_Rect){PANEL_X + 90, PANEL_Y + 246, PANEL_W - 180, 52};

    draw_field(th, "Port", g_app.host_port, &g_app.field_rects[0], g_app.active_field == 0, false);
    draw_field(th, "Passphrase (blank = default)", g_app.passphrase, &g_app.field_rects[1], g_app.active_field == 1, true);
    draw_text_line(g_font_small ? g_font_small : g_font, "Tab/Up/Down switch field | Enter host | Esc back",
                   WINDOW_WIDTH / 2, WINDOW_HEIGHT - 84, th->muted, true);
}

static void draw_join_form(const GuiTheme* th) {
    draw_base(th, "Join LAN Game");

    g_app.field_count = 3;
    g_app.field_rects[0] = (SDL_Rect){PANEL_X + 90, PANEL_Y + 104, PANEL_W - 180, 52};
    g_app.field_rects[1] = (SDL_Rect){PANEL_X + 90, PANEL_Y + 214, PANEL_W - 180, 52};
    g_app.field_rects[2] = (SDL_Rect){PANEL_X + 90, PANEL_Y + 324, PANEL_W - 180, 52};

    draw_field(th, "Host IP", g_app.join_ip, &g_app.field_rects[0], g_app.active_field == 0, false);
    draw_field(th, "Port", g_app.join_port, &g_app.field_rects[1], g_app.active_field == 1, false);
    draw_field(th, "Passphrase (blank = default)", g_app.passphrase, &g_app.field_rects[2], g_app.active_field == 2, true);
    draw_text_line(g_font_small ? g_font_small : g_font, "Tab/Up/Down switch field | Enter connect | Esc back",
                   WINDOW_WIDTH / 2, WINDOW_HEIGHT - 84, th->muted, true);
}

static const char* theme_label(int theme) {
    if (theme == 1) return "Theme: Dark";
    if (theme == 2) return "Theme: Light";
    if (theme == 3) return "Theme: Retro";
    return "Theme: Default";
}

static const char* timer_label(const Config* cfg, char* out, size_t out_sz) {
    if (!cfg || !out) return "Timer: Off";
    if (!cfg->timer_enabled || cfg->timer_seconds <= 0) {
        (void)snprintf(out, out_sz, "Timer: Off");
    } else {
        (void)snprintf(out, out_sz, "Timer: %ds", cfg->timer_seconds);
    }
    return out;
}

static void draw_settings(const GuiTheme* th) {
    char o0[64], o1[64], o2[64], o3[64], o4[64], timer[64];
    const Config* c = g_app.cfg;

    (void)snprintf(o0, sizeof(o0), "Board Size: %dx%d", c->board_size, c->board_size);
    (void)snprintf(o1, sizeof(o1), "%s", theme_label(c->color_theme));
    (void)snprintf(o2, sizeof(o2), "%s", timer_label(c, timer, sizeof(timer)));
    (void)snprintf(o3, sizeof(o3), "Player Symbol: %c", c->player_symbol == 'O' ? 'O' : 'X');
    (void)snprintf(o4, sizeof(o4), "Sound: %s", c->sound_enabled ? "On" : "Off");

    const char* items[6] = {o0, o1, o2, o3, o4, "Back"};
    Button b[6];

    draw_base(th, "Settings");
    layout_buttons(items, 6, 170, b);
    for (int i = 0; i < 6; i++) draw_button(th, &b[i], i == g_app.sel_settings);

    draw_text_line(g_font_small ? g_font_small : g_font, get_config_path(), WINDOW_WIDTH / 2, WINDOW_HEIGHT - 84, th->muted, true);
}

static void draw_scores(const GuiTheme* th) {
    int wins = g_app.score ? g_app.score->wins : 0;
    int losses = g_app.score ? g_app.score->losses : 0;
    int draws = g_app.score ? g_app.score->draws : 0;
    int total = wins + losses + draws;
    double rate = total > 0 ? (100.0 * (double)wins / (double)total) : 0.0;
    char line[96];

    draw_base(th, "High Scores");

    int y = PANEL_Y + 120;
    (void)snprintf(line, sizeof(line), "Wins: %d", wins);
    draw_text_line(g_font, line, WINDOW_WIDTH / 2, y, th->x_color, true);
    y += 58;

    (void)snprintf(line, sizeof(line), "Losses: %d", losses);
    draw_text_line(g_font, line, WINDOW_WIDTH / 2, y, th->err_color, true);
    y += 58;

    (void)snprintf(line, sizeof(line), "Draws: %d", draws);
    draw_text_line(g_font, line, WINDOW_WIDTH / 2, y, th->o_color, true);
    y += 58;

    (void)snprintf(line, sizeof(line), "Games: %d", total);
    draw_text_line(g_font, line, WINDOW_WIDTH / 2, y, th->text, true);
    y += 58;

    (void)snprintf(line, sizeof(line), "Win Rate: %.1f%%", rate);
    draw_text_line(g_font, line, WINDOW_WIDTH / 2, y, th->accent, true);

    draw_text_line(g_font_small ? g_font_small : g_font, "Enter/Esc back to main", WINDOW_WIDTH / 2, WINDOW_HEIGHT - 84, th->muted, true);
}

static void draw_about(const GuiTheme* th) {
    char line[128];
    int y = PANEL_Y + 120;

    draw_base(th, "About");

    (void)snprintf(line, sizeof(line), "Game: %s", APP_NAME);
    draw_text_line(g_font, line, WINDOW_WIDTH / 2, y, th->text, true);
    y += 50;

    (void)snprintf(line, sizeof(line), "Version: v%s", APP_VERSION);
    draw_text_line(g_font, line, WINDOW_WIDTH / 2, y, th->text, true);
    y += 50;

    (void)snprintf(line, sizeof(line), "Author: %s", APP_AUTHOR);
    draw_text_line(g_font, line, WINDOW_WIDTH / 2, y, th->text, true);
    y += 50;

    (void)snprintf(line, sizeof(line), "Contact: %s", APP_CONTACT);
    draw_text_line(g_font, line, WINDOW_WIDTH / 2, y, th->text, true);
    y += 62;

    draw_text_line(g_font_small ? g_font_small : g_font, "CLI + GUI, AI, LAN, Replay, Settings", WINDOW_WIDTH / 2, y, th->muted, true);
    draw_text_line(g_font_small ? g_font_small : g_font, "Enter/Esc back to main", WINDOW_WIDTH / 2, WINDOW_HEIGHT - 84, th->muted, true);
}

static const char* mode_name(const Game* g) {
    if (!g) return "";
    switch (g->mode) {
        case MODE_AI_EASY: return "Easy AI";
        case MODE_AI_MEDIUM: return "Medium AI";
        case MODE_AI_HARD: return "Hard AI";
        case MODE_NETWORK_HOST: return "LAN Host";
        case MODE_NETWORK_CLIENT: return "LAN Client";
        default: return "Two Player Local";
    }
}

static void draw_game(const GuiTheme* th) {
    char line[96];
    draw_base(th, mode_name(&g_app.game));

    draw_board(th, &g_app.game);

    if (g_app.game.state == GAME_STATE_PLAYING || g_app.game.state == GAME_STATE_WAITING) {
        char symbol = (g_app.game.current_player == PLAYER_X) ? g_app.game.symbol_x : g_app.game.symbol_o;
        (void)snprintf(line, sizeof(line), "%c's turn", symbol);
        draw_text_line(g_font, line, WINDOW_WIDTH / 2, 124, g_app.game.current_player == PLAYER_X ? th->x_color : th->o_color, true);
    }

    if (g_app.game.timer_enabled && (g_app.game.state == GAME_STATE_PLAYING || g_app.game.state == GAME_STATE_WAITING)) {
        (void)snprintf(line, sizeof(line), "Timer: %ds", game_get_timer_remaining(&g_app.game));
        draw_text_line(g_font_small ? g_font_small : g_font, line, WINDOW_WIDTH / 2, 148, th->muted, true);
    }

    draw_text_line(g_font_small ? g_font_small : g_font,
                   "Click cell to move | Esc main menu",
                   WINDOW_WIDTH / 2,
                   WINDOW_HEIGHT - 84,
                   th->muted,
                   true);

    if (g_app.game.state == GAME_STATE_WIN || g_app.game.state == GAME_STATE_DRAW) {
        SDL_Rect ov = {PANEL_X + 140, WINDOW_HEIGHT - 176, PANEL_W - 280, 84};
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 10, 14, 20, 210);
        SDL_RenderFillRect(g_renderer, &ov);
        SDL_SetRenderDrawColor(g_renderer, th->border.r, th->border.g, th->border.b, 255);
        SDL_RenderDrawRect(g_renderer, &ov);

        if (g_app.game.state == GAME_STATE_DRAW) {
            draw_text_line(g_font, "Draw game", WINDOW_WIDTH / 2, ov.y + 12, th->o_color, true);
        } else {
            Player w = game_get_winner(&g_app.game);
            char s = (w == PLAYER_X) ? g_app.game.symbol_x : g_app.game.symbol_o;
            (void)snprintf(line, sizeof(line), "%c wins", s);
            draw_text_line(g_font, line, WINDOW_WIDTH / 2, ov.y + 12, th->win_color, true);
        }

        draw_text_line(g_font_small ? g_font_small : g_font,
                       g_app.game_network ? "Enter/Esc return main" : "Enter rematch | Esc main",
                       WINDOW_WIDTH / 2,
                       ov.y + 46,
                       th->muted,
                       true);
    }
}

static void render_current_screen(void) {
    GuiTheme th;
    set_theme(g_app.cfg, &th);

    if (g_app.screen == SCREEN_MAIN) draw_main(&th);
    else if (g_app.screen == SCREEN_AI) draw_ai(&th);
    else if (g_app.screen == SCREEN_NETWORK) draw_network(&th);
    else if (g_app.screen == SCREEN_NET_HOST) draw_host_form(&th);
    else if (g_app.screen == SCREEN_NET_JOIN) draw_join_form(&th);
    else if (g_app.screen == SCREEN_NET_WAIT) draw_waiting_host(&th);
    else if (g_app.screen == SCREEN_SETTINGS) draw_settings(&th);
    else if (g_app.screen == SCREEN_SCORES) draw_scores(&th);
    else if (g_app.screen == SCREEN_ABOUT) draw_about(&th);
    else if (g_app.screen == SCREEN_GAME) draw_game(&th);

    draw_status(&g_app);
    SDL_RenderPresent(g_renderer);
}

static void enter_text_mode(bool on) {
    if (on && !g_app.text_input) {
        SDL_StartTextInput();
        g_app.text_input = true;
    } else if (!on && g_app.text_input) {
        SDL_StopTextInput();
        g_app.text_input = false;
    }
}

static void set_screen(GuiScreen s) {
    g_app.screen = s;
    if (s == SCREEN_NET_HOST || s == SCREEN_NET_JOIN) {
        if (g_app.active_field < 0) g_app.active_field = 0;
        enter_text_mode(true);
    } else {
        g_app.active_field = -1;
        g_app.field_count = 0;
        enter_text_mode(false);
    }
}

static void back_to_main_from_game(void) {
    if (g_app.game_network) {
        close_network(&g_app);
    }
    g_app.game_active = false;
    g_app.game_network = false;
    set_screen(SCREEN_MAIN);
}

static void restart_game_non_network(void) {
    GameMode mode = g_app.game.mode;
    start_game(&g_app, mode);
}

static void cycle_setting(int idx) {
    static const int timer_cycle[] = {0, 10, 15, 30, 45, 60};
    const int timer_count = (int)(sizeof(timer_cycle) / sizeof(timer_cycle[0]));

    if (!g_app.cfg) return;

    if (idx == 0) {
        int next = g_app.cfg->board_size + 1;
        if (next > MAX_BOARD_SIZE) next = MIN_BOARD_SIZE;
        g_app.cfg->board_size = next;
    } else if (idx == 1) {
        int next = g_app.cfg->color_theme + 1;
        if (next > 3) next = 0;
        g_app.cfg->color_theme = next;
    } else if (idx == 2) {
        int current = (g_app.cfg->timer_enabled && g_app.cfg->timer_seconds > 0) ? g_app.cfg->timer_seconds : 0;
        int i = 0;
        for (; i < timer_count; i++) {
            if (timer_cycle[i] == current) break;
        }
        i = (i + 1) % timer_count;
        g_app.cfg->timer_seconds = timer_cycle[i];
        g_app.cfg->timer_enabled = (timer_cycle[i] > 0);
    } else if (idx == 3) {
        g_app.cfg->player_symbol = (g_app.cfg->player_symbol == 'O') ? 'X' : 'O';
    } else if (idx == 4) {
        g_app.cfg->sound_enabled = !g_app.cfg->sound_enabled;
        if (g_app.sound) sound_set_enabled(g_app.sound, g_app.cfg->sound_enabled);
    }

    if (!config_save(g_app.cfg, get_config_path())) {
        set_status(&g_app, "Failed to save config", (SDL_Color){255, 112, 112, 255}, 2200);
    } else {
        set_status(&g_app, "Settings saved", (SDL_Color){124, 255, 170, 255}, 1200);
    }
}

static void activate_main(void) {
    if (g_app.sound) sound_play(g_app.sound, SOUND_MENU);

    if (g_app.sel_main == 0) set_screen(SCREEN_AI);
    else if (g_app.sel_main == 1) start_game(&g_app, MODE_LOCAL_2P);
    else if (g_app.sel_main == 2) set_screen(SCREEN_NETWORK);
    else if (g_app.sel_main == 3) set_screen(SCREEN_SCORES);
    else if (g_app.sel_main == 4) set_screen(SCREEN_SETTINGS);
    else if (g_app.sel_main == 5) set_screen(SCREEN_ABOUT);
    else if (g_app.sel_main == 6) g_app.running = false;
}

static void activate_ai(void) {
    if (g_app.sound) sound_play(g_app.sound, SOUND_MENU);

    if (g_app.sel_ai == 3) {
        set_screen(SCREEN_MAIN);
        return;
    }

    if (g_app.sel_ai == 0) start_game(&g_app, MODE_AI_EASY);
    else if (g_app.sel_ai == 1) start_game(&g_app, MODE_AI_MEDIUM);
    else start_game(&g_app, MODE_AI_HARD);
}

static void activate_network(void) {
    if (g_app.sound) sound_play(g_app.sound, SOUND_MENU);

    if (g_app.sel_net == 0) {
        g_app.active_field = 0;
        set_screen(SCREEN_NET_HOST);
    } else if (g_app.sel_net == 1) {
        g_app.active_field = 0;
        set_screen(SCREEN_NET_JOIN);
    } else {
        set_screen(SCREEN_MAIN);
    }
}

static bool start_host(void) {
    close_network(&g_app);

    if (!network_init(&g_app.net)) {
        set_status(&g_app, "Network init failed", (SDL_Color){255, 112, 112, 255}, 2400);
        return false;
    }
    g_app.net_init = true;

    normalize_port(g_app.host_port, sizeof(g_app.host_port));
    int port = parse_port(g_app.host_port);

    network_set_passphrase(&g_app.net, g_app.passphrase);
    if (!network_host(&g_app.net, port)) {
        close_network(&g_app);
        set_status(&g_app, "Unable to host game", (SDL_Color){255, 112, 112, 255}, 2400);
        return false;
    }

    g_app.net_waiting = true;
    g_app.net_wait_since = SDL_GetTicks();
    set_screen(SCREEN_NET_WAIT);

    char msg[160];
    (void)snprintf(msg, sizeof(msg), "Hosting on port %d", port);
    set_status(&g_app, msg, (SDL_Color){128, 226, 255, 255}, 2500);
    return true;
}

static bool start_join(void) {
    close_network(&g_app);

    if (!network_init(&g_app.net)) {
        set_status(&g_app, "Network init failed", (SDL_Color){255, 112, 112, 255}, 2400);
        return false;
    }
    g_app.net_init = true;

    normalize_port(g_app.join_port, sizeof(g_app.join_port));
    int port = parse_port(g_app.join_port);

    network_set_passphrase(&g_app.net, g_app.passphrase);
    if (!network_connect(&g_app.net, g_app.join_ip, port)) {
        close_network(&g_app);
        set_status(&g_app, "Connection failed", (SDL_Color){255, 112, 112, 255}, 2600);
        return false;
    }

    if (!network_secure_handshake(&g_app.net, 10000)) {
        close_network(&g_app);
        set_status(&g_app, "Secure handshake failed", (SDL_Color){255, 112, 112, 255}, 2800);
        return false;
    }

    start_network_game(&g_app, false);
    return true;
}

static void append_field_text(const char* txt) {
    char* dst = NULL;
    size_t cap = 0;

    if (g_app.screen == SCREEN_NET_HOST) {
        if (g_app.active_field == 0) {
            dst = g_app.host_port;
            cap = sizeof(g_app.host_port);
        } else if (g_app.active_field == 1) {
            dst = g_app.passphrase;
            cap = sizeof(g_app.passphrase);
        }
    } else if (g_app.screen == SCREEN_NET_JOIN) {
        if (g_app.active_field == 0) {
            dst = g_app.join_ip;
            cap = sizeof(g_app.join_ip);
        } else if (g_app.active_field == 1) {
            dst = g_app.join_port;
            cap = sizeof(g_app.join_port);
        } else if (g_app.active_field == 2) {
            dst = g_app.passphrase;
            cap = sizeof(g_app.passphrase);
        }
    }

    if (!dst || cap < 2) return;

    size_t len = strlen(dst);
    for (size_t i = 0; txt[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)txt[i];
        if (len + 1 >= cap) break;

        if (g_app.screen == SCREEN_NET_JOIN && g_app.active_field == 0) {
            if (!(isdigit(ch) || ch == '.')) continue;
        } else if ((g_app.screen == SCREEN_NET_HOST && g_app.active_field == 0) ||
                   (g_app.screen == SCREEN_NET_JOIN && g_app.active_field == 1)) {
            if (!isdigit(ch)) continue;
        } else {
            if (!isprint(ch)) continue;
        }

        dst[len++] = (char)ch;
        dst[len] = '\0';
    }
}

static void backspace_field(void) {
    char* dst = NULL;

    if (g_app.screen == SCREEN_NET_HOST) {
        dst = (g_app.active_field == 0) ? g_app.host_port : (g_app.active_field == 1 ? g_app.passphrase : NULL);
    } else if (g_app.screen == SCREEN_NET_JOIN) {
        if (g_app.active_field == 0) dst = g_app.join_ip;
        else if (g_app.active_field == 1) dst = g_app.join_port;
        else if (g_app.active_field == 2) dst = g_app.passphrase;
    }

    if (!dst) return;
    size_t len = strlen(dst);
    if (len > 0) dst[len - 1] = '\0';
}

static void handle_mouse_main(int x, int y) {
    Button b[7];
    layout_buttons(MAIN_OPTIONS, 7, 185, b);
    for (int i = 0; i < 7; i++) {
        if (in_rect(x, y, &b[i].rect)) {
            g_app.sel_main = i;
            activate_main();
            return;
        }
    }
}

static void handle_mouse_ai(int x, int y) {
    Button b[4];
    layout_buttons(AI_OPTIONS, 4, 230, b);
    for (int i = 0; i < 4; i++) {
        if (in_rect(x, y, &b[i].rect)) {
            g_app.sel_ai = i;
            activate_ai();
            return;
        }
    }
}

static void handle_mouse_net(int x, int y) {
    Button b[3];
    layout_buttons(NET_OPTIONS, 3, 250, b);
    for (int i = 0; i < 3; i++) {
        if (in_rect(x, y, &b[i].rect)) {
            g_app.sel_net = i;
            activate_network();
            return;
        }
    }
}

static void handle_mouse_settings(int x, int y) {
    char o0[64], o1[64], o2[64], o3[64], o4[64], timer[64];
    const Config* c = g_app.cfg;

    (void)snprintf(o0, sizeof(o0), "Board Size: %dx%d", c->board_size, c->board_size);
    (void)snprintf(o1, sizeof(o1), "%s", theme_label(c->color_theme));
    (void)snprintf(o2, sizeof(o2), "%s", timer_label(c, timer, sizeof(timer)));
    (void)snprintf(o3, sizeof(o3), "Player Symbol: %c", c->player_symbol == 'O' ? 'O' : 'X');
    (void)snprintf(o4, sizeof(o4), "Sound: %s", c->sound_enabled ? "On" : "Off");

    const char* items[6] = {o0, o1, o2, o3, o4, "Back"};
    Button b[6];
    layout_buttons(items, 6, 170, b);

    for (int i = 0; i < 6; i++) {
        if (in_rect(x, y, &b[i].rect)) {
            g_app.sel_settings = i;
            if (i == 5) set_screen(SCREEN_MAIN);
            else cycle_setting(i);
            return;
        }
    }
}

static void handle_mouse_game(int x, int y) {
    if (!g_app.game_active) return;
    if (!(g_app.game.state == GAME_STATE_PLAYING || g_app.game.state == GAME_STATE_WAITING)) return;

    if (g_app.game.mode >= MODE_AI_EASY && g_app.game.mode <= MODE_AI_HARD &&
        g_app.game.current_player != g_app.game.player_symbol) return;
    if (g_app.game_network && g_app.game.current_player != g_app.game.player_symbol) return;

    uint8_t r = 0, c = 0;
    if (!cell_from_mouse(&g_app.game, x, y, &r, &c)) return;

    if (!game_make_move(&g_app.game, r, c)) {
        if (g_app.sound) sound_play(g_app.sound, SOUND_INVALID);
        return;
    }

    if (g_app.sound) sound_play(g_app.sound, SOUND_MOVE);

    if (g_app.game_network && !network_send_move(&g_app.net, r, c)) {
        set_status(&g_app, "Connection lost", (SDL_Color){255, 112, 112, 255}, 2400);
        back_to_main_from_game();
        return;
    }

    if (g_app.game.mode >= MODE_AI_EASY && g_app.game.mode <= MODE_AI_HARD &&
        g_app.game.state == GAME_STATE_PLAYING &&
        g_app.game.current_player != g_app.game.player_symbol) {
        g_app.ai_tick = SDL_GetTicks() + 360;
    }
}

static void handle_key(SDL_Keycode key) {
    if (key == SDLK_ESCAPE) {
        if (g_app.screen == SCREEN_MAIN) {
            g_app.running = false;
            return;
        }

        if (g_app.screen == SCREEN_GAME) {
            back_to_main_from_game();
            return;
        }

        if (g_app.screen == SCREEN_NET_WAIT) {
            close_network(&g_app);
            set_screen(SCREEN_NETWORK);
            return;
        }

        set_screen(SCREEN_MAIN);
        return;
    }

    if (g_app.screen == SCREEN_MAIN) {
        if (key == SDLK_UP) g_app.sel_main = (g_app.sel_main + 6) % 7;
        else if (key == SDLK_DOWN) g_app.sel_main = (g_app.sel_main + 1) % 7;
        else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) activate_main();
        return;
    }

    if (g_app.screen == SCREEN_AI) {
        if (key == SDLK_UP) g_app.sel_ai = (g_app.sel_ai + 3) % 4;
        else if (key == SDLK_DOWN) g_app.sel_ai = (g_app.sel_ai + 1) % 4;
        else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) activate_ai();
        return;
    }

    if (g_app.screen == SCREEN_NETWORK) {
        if (key == SDLK_UP) g_app.sel_net = (g_app.sel_net + 2) % 3;
        else if (key == SDLK_DOWN) g_app.sel_net = (g_app.sel_net + 1) % 3;
        else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) activate_network();
        return;
    }

    if (g_app.screen == SCREEN_NET_HOST) {
        if (key == SDLK_TAB || key == SDLK_DOWN || key == SDLK_UP) g_app.active_field = (g_app.active_field + 1) % 2;
        else if (key == SDLK_BACKSPACE) backspace_field();
        else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) (void)start_host();
        return;
    }

    if (g_app.screen == SCREEN_NET_JOIN) {
        if (key == SDLK_TAB || key == SDLK_DOWN) g_app.active_field = (g_app.active_field + 1) % 3;
        else if (key == SDLK_UP) g_app.active_field = (g_app.active_field + 2) % 3;
        else if (key == SDLK_BACKSPACE) backspace_field();
        else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) (void)start_join();
        return;
    }

    if (g_app.screen == SCREEN_SETTINGS) {
        if (key == SDLK_UP) g_app.sel_settings = (g_app.sel_settings + 5) % 6;
        else if (key == SDLK_DOWN) g_app.sel_settings = (g_app.sel_settings + 1) % 6;
        else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            if (g_app.sel_settings == 5) set_screen(SCREEN_MAIN);
            else cycle_setting(g_app.sel_settings);
        }
        return;
    }

    if (g_app.screen == SCREEN_SCORES || g_app.screen == SCREEN_ABOUT) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) set_screen(SCREEN_MAIN);
        return;
    }

    if (g_app.screen == SCREEN_GAME) {
        if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) &&
            (g_app.game.state == GAME_STATE_WIN || g_app.game.state == GAME_STATE_DRAW)) {
            if (g_app.game_network) back_to_main_from_game();
            else restart_game_non_network();
        }
    }
}

static void handle_event(SDL_Event* e) {
    if (e->type == SDL_QUIT) {
        g_app.running = false;
        return;
    }

    if (e->type == SDL_KEYDOWN) {
        handle_key(e->key.keysym.sym);
        return;
    }

    if (e->type == SDL_TEXTINPUT) {
        append_field_text(e->text.text);
        return;
    }

    if (e->type != SDL_MOUSEBUTTONDOWN || e->button.button != SDL_BUTTON_LEFT) {
        return;
    }

    int x = e->button.x;
    int y = e->button.y;

    if (g_app.screen == SCREEN_MAIN) handle_mouse_main(x, y);
    else if (g_app.screen == SCREEN_AI) handle_mouse_ai(x, y);
    else if (g_app.screen == SCREEN_NETWORK) handle_mouse_net(x, y);
    else if (g_app.screen == SCREEN_SETTINGS) handle_mouse_settings(x, y);
    else if (g_app.screen == SCREEN_GAME) handle_mouse_game(x, y);
    else if (g_app.screen == SCREEN_NET_HOST || g_app.screen == SCREEN_NET_JOIN) {
        for (int i = 0; i < g_app.field_count; i++) {
            if (in_rect(x, y, &g_app.field_rects[i])) {
                g_app.active_field = i;
                return;
            }
        }
    }
}

static void update_network_wait(void) {
    if (!g_app.net_waiting || !g_app.net_init) return;

    if (SDL_GetTicks() - g_app.net_wait_since > 120000U) {
        close_network(&g_app);
        set_screen(SCREEN_NETWORK);
        set_status(&g_app, "Host wait timeout", (SDL_Color){255, 112, 112, 255}, 2400);
        return;
    }

    if (!g_app.net.connected) {
        (void)network_accept(&g_app.net, 0);
        return;
    }

    if (!network_secure_handshake(&g_app.net, 10000)) {
        close_network(&g_app);
        set_screen(SCREEN_NETWORK);
        set_status(&g_app, "Secure handshake failed", (SDL_Color){255, 112, 112, 255}, 2600);
        return;
    }

    g_app.net_waiting = false;
    start_network_game(&g_app, true);
}

static void update_game(void) {
    if (!g_app.game_active) return;

    if ((g_app.game.state == GAME_STATE_PLAYING || g_app.game.state == GAME_STATE_WAITING) && g_app.game.timer_enabled) {
        uint32_t now = SDL_GetTicks();
        while (now - g_app.timer_tick >= 1000U &&
               (g_app.game.state == GAME_STATE_PLAYING || g_app.game.state == GAME_STATE_WAITING)) {
            (void)game_update_timer(&g_app.game);
            g_app.timer_tick += 1000U;
        }
    }

    if (g_app.game.mode >= MODE_AI_EASY && g_app.game.mode <= MODE_AI_HARD &&
        g_app.game.state == GAME_STATE_PLAYING &&
        g_app.game.current_player != g_app.game.player_symbol &&
        SDL_GetTicks() >= g_app.ai_tick) {
        Move m;
        ai_get_move(&g_app.game, &m);
        if (m.row < g_app.game.size && m.col < g_app.game.size && game_make_move(&g_app.game, m.row, m.col)) {
            if (g_app.sound) sound_play(g_app.sound, SOUND_MOVE);
        }
        g_app.ai_tick = SDL_GetTicks() + 500;
    }

    if (g_app.game_network &&
        (g_app.game.state == GAME_STATE_PLAYING || g_app.game.state == GAME_STATE_WAITING) &&
        g_app.game.current_player != g_app.game.player_symbol) {
        uint8_t r = 0, c = 0;
        if (network_receive_move(&g_app.net, &r, &c, 0)) {
            if (game_make_move(&g_app.game, r, c) && g_app.sound) sound_play(g_app.sound, SOUND_MOVE);
        } else if (!g_app.net.connected) {
            set_status(&g_app, "Opponent disconnected", (SDL_Color){255, 112, 112, 255}, 2400);
            back_to_main_from_game();
            return;
        }
    }

    save_scores_if_done(&g_app);
}

bool gui_init(GUIState* gui) {
    if (!gui) return false;

    gui->initialized = false;
    gui->use_gui = false;
    gui->window_width = WINDOW_WIDTH;
    gui->window_height = WINDOW_HEIGHT;
    gui->cell_size = 120;
    gui->board_offset_x = (WINDOW_WIDTH - 360) / 2;
    gui->board_offset_y = 170;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;
    if (TTF_Init() < 0) {
        SDL_Quit();
        return false;
    }

    g_window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!g_window) {
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    g_font = try_open_font(30);
    g_font_small = try_open_font(20);
    g_font_big = try_open_font(46);

    if (!g_font) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    if (!g_font_small) g_font_small = g_font;
    if (!g_font_big) g_font_big = g_font;

    gui->initialized = true;
    gui->use_gui = true;
    g_gui = gui;
    return true;
}

void gui_close(GUIState* gui) {
    (void)gui;

    close_network(&g_app);
    enter_text_mode(false);

    if (g_font_big && g_font_big != g_font) TTF_CloseFont(g_font_big);
    if (g_font_small && g_font_small != g_font) TTF_CloseFont(g_font_small);
    if (g_font) TTF_CloseFont(g_font);

    g_font_big = NULL;
    g_font_small = NULL;
    g_font = NULL;

    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);

    g_renderer = NULL;
    g_window = NULL;
    g_gui = NULL;

    TTF_Quit();
    SDL_Quit();
}

bool gui_run_app(Config* cfg, Score* score, Sound* sound) {
    if (!cfg || !score || !g_window || !g_renderer) return false;

    memset(&g_app, 0, sizeof(g_app));
    g_app.cfg = cfg;
    g_app.score = score;
    g_app.sound = sound;
    g_app.running = true;

    (void)snprintf(g_app.host_port, sizeof(g_app.host_port), "%d", DEFAULT_PORT);
    (void)snprintf(g_app.join_port, sizeof(g_app.join_port), "%d", DEFAULT_PORT);
    (void)snprintf(g_app.join_ip, sizeof(g_app.join_ip), "127.0.0.1");

    set_screen(SCREEN_MAIN);

    while (g_app.running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            handle_event(&e);
        }

        if (g_app.screen == SCREEN_NET_WAIT) update_network_wait();
        if (g_app.screen == SCREEN_GAME) update_game();

        render_current_screen();
        SDL_Delay(16);
    }

    close_network(&g_app);
    enter_text_mode(false);
    return true;
}

bool gui_run_game(Game* game) {
    if (!game || !g_window || !g_renderer) return false;

    Config cfg;
    Score score;
    Sound sound;

    config_init(&cfg);
    score_init(&score);
    sound_init(&sound);

    memset(&g_app, 0, sizeof(g_app));
    g_app.cfg = &cfg;
    g_app.score = &score;
    g_app.sound = &sound;
    g_app.game = *game;
    g_app.game_active = true;
    g_app.running = true;
    set_screen(SCREEN_GAME);
    g_app.timer_tick = SDL_GetTicks();

    while (g_app.running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) handle_event(&e);
        update_game();
        render_current_screen();
        SDL_Delay(16);
    }

    *game = g_app.game;
    return true;
}

void gui_draw_board(Game* game, SDL_Renderer* renderer, SDL_Texture* font_texture) {
    (void)renderer;
    (void)font_texture;
    if (!game || !g_renderer) return;

    GuiTheme th;
    set_theme(g_app.cfg, &th);
    draw_base(&th, mode_name(game));
    draw_board(&th, game);
    SDL_RenderPresent(g_renderer);
}

void gui_handle_click(Game* game, int mouse_x, int mouse_y) {
    if (!game) return;
    uint8_t r = 0, c = 0;
    if (cell_from_mouse(game, mouse_x, mouse_y, &r, &c)) {
        (void)game_make_move(game, r, c);
    }
}

void gui_draw_game_over(Game* game, SDL_Renderer* renderer) {
    (void)renderer;
    if (!game || !g_renderer) return;

    GuiTheme th;
    set_theme(g_app.cfg, &th);
    draw_base(&th, mode_name(game));
    draw_board(&th, game);

    if (game->state == GAME_STATE_DRAW) {
        draw_text_line(g_font_big ? g_font_big : g_font, "Draw game", WINDOW_WIDTH / 2, WINDOW_HEIGHT - 140, th.o_color, true);
    } else if (game->state == GAME_STATE_WIN) {
        Player w = game_get_winner(game);
        char s = (w == PLAYER_X) ? game->symbol_x : game->symbol_o;
        char line[64];
        (void)snprintf(line, sizeof(line), "%c wins", s);
        draw_text_line(g_font_big ? g_font_big : g_font, line, WINDOW_WIDTH / 2, WINDOW_HEIGHT - 140, th.win_color, true);
    }

    SDL_RenderPresent(g_renderer);
}

#endif
