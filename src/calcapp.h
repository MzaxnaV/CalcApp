#ifndef CALCAPP_H
#define CALCAPP_H

#include "calcapp_pch.h"

// Grid config
constexpr int ROWS   = 4;
constexpr int COLS   = 4;
constexpr int INPUTS = 8; // 4 row sums + 4 col sums

// Window / layout
constexpr int WIN_W   = 640;
constexpr int WIN_H   = 560;
constexpr int CELL_W  = 80;
constexpr int CELL_H  = 60;
constexpr int GRID_X  = 80;
constexpr int GRID_Y  = 120;
constexpr int FONT_SZ = 22;

// All 8 lap inputs stacked in a column on the right
// Row sums: x = GRID_X + COLS*CELL_W = 400, w = CELL_W+20 = 100, right edge = 500
constexpr int LAP_X = GRID_X + COLS * CELL_W + (CELL_W + 20) + 4; // 504
constexpr int LAP_W = WIN_W - LAP_X - 6;                           // 130
constexpr int LAP_H = 30;
constexpr int LAP_GAP = 2; // gap between stacked lap inputs

// Derived vertical positions (col sums end at GRID_Y + (ROWS+1)*CELL_H = 420)
constexpr int BTN_Y = GRID_Y + (ROWS + 1) * CELL_H + 10; // 430
constexpr int MSG_Y = BTN_Y + 50;                          // 480

struct Input {
    char text[8];
    int  correct_val;
    bool checked;
    bool is_correct;
};

struct Session {
    char   date[16];
    double laps[INPUTS]; // individual lap times; 0 = not entered
    int    correct;
};

struct Theme {
    Color bg;
    Color surface;
    Color border;
    Color text_pri;
    Color text_mut;
    Color active_border;
    Color correct;
    Color wrong;
    Color green;
    Color red;
};

// Layout helpers
Rectangle cell_rect(int r, int c);
Rectangle row_input_rect(int r);
Rectangle col_input_rect(int c);

// Draw helpers
void draw_centered_text(const char *txt, Rectangle rect, int sz, Color col);

// Game logic
void new_table();
void check_answers();
void handle_text_input(Input *inp);

#endif // CALCAPP_H
