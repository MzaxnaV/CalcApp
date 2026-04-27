#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define ROWS 4
#define COLS 4
#define INPUTS 8   /* 4 row sums + 4 col sums */

/* Layout */
#define WIN_W   640
#define WIN_H   560
#define CELL_W  80
#define CELL_H  60
#define GRID_X  80
#define GRID_Y  120
#define FONT_SZ 22

typedef struct {
    char text[8];
    int  correct_val;
    bool checked;
    bool is_correct;
} Input;

static int    grid[ROWS][COLS];
static Input  inputs[INPUTS];   /* [0..3] = row sums, [4..7] = col sums */
static int    active   = 0;     /* index of focused input */
static double start_t  = 0.0;
static double elapsed  = 0.0;
static bool   timing   = false;
static bool   done     = false;
static double best     = -1.0;
static int    attempts = 0;
static char   msg[64]  = "";

static int row_sum(int r) {
    int s = 0;
    for (int c = 0; c < COLS; c++) s += grid[r][c];
    return s;
}

static int col_sum(int c) {
    int s = 0;
    for (int r = 0; r < ROWS; r++) s += grid[r][c];
    return s;
}

static void new_table(void) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            grid[r][c] = 10 + rand() % 90;

    for (int i = 0; i < ROWS; i++) {
        inputs[i].correct_val = row_sum(i);
        inputs[i].text[0]     = '\0';
        inputs[i].checked     = false;
        inputs[i].is_correct  = false;
    }
    for (int i = 0; i < COLS; i++) {
        inputs[ROWS + i].correct_val = col_sum(i);
        inputs[ROWS + i].text[0]     = '\0';
        inputs[ROWS + i].checked     = false;
        inputs[ROWS + i].is_correct  = false;
    }

    active  = 0;
    timing  = false;
    done    = false;
    elapsed = 0.0;
    msg[0]  = '\0';
}

static void check_answers(void) {
    /* require all filled */
    for (int i = 0; i < INPUTS; i++) {
        if (inputs[i].text[0] == '\0') {
            snprintf(msg, sizeof(msg), "Fill all 8 answers first.");
            return;
        }
    }
    if (timing) {
        elapsed = GetTime() - start_t;
        timing  = false;
    }
    done = true;
    attempts++;

    int correct = 0;
    for (int i = 0; i < INPUTS; i++) {
        inputs[i].checked    = true;
        int val              = atoi(inputs[i].text);
        inputs[i].is_correct = (val == inputs[i].correct_val);
        if (inputs[i].is_correct) correct++;
        else snprintf(inputs[i].text, sizeof(inputs[i].text),
                      "%d", inputs[i].correct_val);
    }

    if (correct == INPUTS) {
        if (best < 0.0 || elapsed < best) best = elapsed;
        if      (elapsed <= 30.0) snprintf(msg, sizeof(msg), "All correct in %.1fs — Excellent!", elapsed);
        else if (elapsed <= 60.0) snprintf(msg, sizeof(msg), "All correct in %.1fs — Good.",      elapsed);
        else                      snprintf(msg, sizeof(msg), "All correct in %.1fs — Keep going.", elapsed);
    } else {
        snprintf(msg, sizeof(msg), "%d wrong. Corrections shown.", INPUTS - correct);
    }
}

/* Returns pixel rect for a grid cell (number display) */
static Rectangle cell_rect(int r, int c) {
    return (Rectangle){
        GRID_X + c * CELL_W,
        GRID_Y + r * CELL_H,
        CELL_W, CELL_H
    };
}

/* Returns pixel rect for a row-sum input box (column 4) */
static Rectangle row_input_rect(int r) {
    return (Rectangle){
        GRID_X + COLS * CELL_W,
        GRID_Y + r * CELL_H,
        CELL_W + 20, CELL_H
    };
}

/* Returns pixel rect for a col-sum input box (row 4) */
static Rectangle col_input_rect(int c) {
    return (Rectangle){
        GRID_X + c * CELL_W,
        GRID_Y + ROWS * CELL_H,
        CELL_W, CELL_H
    };
}

static void handle_text_input(Input *inp) {
    int key = GetCharPressed();
    while (key > 0) {
        int len = strlen(inp->text);
        if (key >= '0' && key <= '9' && len < 5) {
            inp->text[len]     = (char)key;
            inp->text[len + 1] = '\0';
            if (!timing && !done) { timing = true; start_t = GetTime(); }
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
        int len = strlen(inp->text);
        if (len > 0) inp->text[len - 1] = '\0';
    }
    if (IsKeyPressed(KEY_TAB)) {
        active = (active + (IsKeyDown(KEY_LEFT_SHIFT) ? INPUTS - 1 : 1)) % INPUTS;
    }
    if (IsKeyPressed(KEY_ENTER)) check_answers();
}

static void draw_centered_text(const char *txt, Rectangle r, int sz, Color col) {
    int tw = MeasureText(txt, sz);
    DrawText(txt, (int)(r.x + (r.width - tw) / 2),
             (int)(r.y + (r.height - sz) / 2), sz, col);
}

int main(void) {
    srand((unsigned)time(NULL));
    InitWindow(WIN_W, WIN_H, "4x4 Addition Speed Test");
    SetTargetFPS(60);
    new_table();

    Color BG       = { 248, 247, 244, 255 };
    Color SURFACE  = { 237, 236, 232, 255 };
    Color BORDER   = { 200, 198, 192, 255 };
    Color TEXT_PRI = {  30,  30,  28, 255 };
    Color TEXT_MUT = { 120, 118, 112, 255 };
    Color ACTIVE_B = {  55,  90, 180, 255 };
    Color CORRECT  = { 234, 243, 222, 255 };
    Color WRONG    = { 252, 235, 235, 255 };
    Color C_GREEN  = {  59, 109,  17, 255 };
    Color C_RED    = { 163,  45,  45, 255 };

    while (!WindowShouldClose()) {
        /* Input */
        if (!done) handle_text_input(&inputs[active]);

        /* Click to focus an input */
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mp = GetMousePosition();
            for (int r = 0; r < ROWS; r++) {
                Rectangle rect = row_input_rect(r);
                if (CheckCollisionPointRec(mp, rect)) { active = r; break; }
            }
            for (int c = 0; c < COLS; c++) {
                Rectangle rect = col_input_rect(c);
                if (CheckCollisionPointRec(mp, rect)) { active = ROWS + c; break; }
            }
            /* Check button */
            Rectangle chk = { GRID_X, GRID_Y + ROWS * CELL_H + 50, 160, 40 };
            if (CheckCollisionPointRec(mp, chk) && !done) check_answers();
            /* New table button */
            Rectangle nw  = { GRID_X + 180, GRID_Y + ROWS * CELL_H + 50, 160, 40 };
            if (CheckCollisionPointRec(mp, nw)) new_table();
        }

        /* Timer display */
        double display_t = timing ? (GetTime() - start_t) : elapsed;

        BeginDrawing();
        ClearBackground(BG);

        /* Title */
        DrawText("4x4 Addition", GRID_X, 24, 20, TEXT_PRI);
        DrawText("Speed Test",   GRID_X + MeasureText("4x4 Addition", 20) + 8, 24, 20, TEXT_MUT);

        /* Stats row */
        char buf[32];
        int sx = WIN_W - 240;
        DrawText("Time",     sx,      28, 11, TEXT_MUT);
        DrawText("Best",     sx + 80, 28, 11, TEXT_MUT);
        DrawText("Attempts", sx +160, 28, 11, TEXT_MUT);

        snprintf(buf, sizeof(buf), "%.1fs", display_t);
        DrawText(buf, sx, 42, 18, TEXT_PRI);

        if (best >= 0.0) snprintf(buf, sizeof(buf), "%.1fs", best);
        else             snprintf(buf, sizeof(buf), "—");
        DrawText(buf, sx + 80, 42, 18, TEXT_PRI);

        snprintf(buf, sizeof(buf), "%d", attempts);
        DrawText(buf, sx + 160, 42, 18, TEXT_PRI);

        /* Column headers */
        for (int c = 0; c < COLS; c++) {
            snprintf(buf, sizeof(buf), "col %d", c + 1);
            Rectangle r = cell_rect(0, c);
            r.y = GRID_Y - 22; r.height = 20;
            draw_centered_text(buf, r, 11, TEXT_MUT);
        }
        /* Row labels */
        for (int r = 0; r < ROWS; r++) {
            snprintf(buf, sizeof(buf), "row %d", r + 1);
            DrawText(buf, GRID_X - 70, (int)(GRID_Y + r * CELL_H + (CELL_H - 11) / 2), 11, TEXT_MUT);
        }
        /* "sum" labels */
        DrawText("sum", GRID_X + COLS * CELL_W + 8, GRID_Y - 22, 11, TEXT_MUT);
        {
            Rectangle r = col_input_rect(0);
            DrawText("sum", GRID_X - 70, (int)(r.y + (CELL_H - 11) / 2), 11, TEXT_MUT);
        }

        /* Grid cells */
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                Rectangle rect = cell_rect(r, c);
                DrawRectangleRec(rect, SURFACE);
                DrawRectangleLinesEx(rect, 0.5f, BORDER);
                snprintf(buf, sizeof(buf), "%d", grid[r][c]);
                draw_centered_text(buf, rect, FONT_SZ, TEXT_PRI);
            }
        }

        /* Input boxes */
        for (int i = 0; i < INPUTS; i++) {
            Rectangle rect = (i < ROWS) ? row_input_rect(i) : col_input_rect(i - ROWS);
            Color bg = SURFACE;
            Color border_col = BORDER;
            Color txt_col = TEXT_PRI;

            if (inputs[i].checked) {
                bg = inputs[i].is_correct ? CORRECT : WRONG;
                txt_col = inputs[i].is_correct ? C_GREEN : C_RED;
            } else if (i == active) {
                border_col = ACTIVE_B;
            }

            DrawRectangleRec(rect, bg);
            DrawRectangleLinesEx(rect, (i == active && !done) ? 1.5f : 0.5f, border_col);

            /* Cursor blink */
            char disp[16];
            if (i == active && !done && (int)(GetTime() * 2) % 2 == 0)
                snprintf(disp, sizeof(disp), "%s|", inputs[i].text);
            else
                snprintf(disp, sizeof(disp), "%s", inputs[i].text[0] ? inputs[i].text : "?");

            Color dc = inputs[i].text[0] ? txt_col : TEXT_MUT;
            draw_centered_text(disp, rect, FONT_SZ, dc);
        }

        /* Buttons */
        Rectangle chk_btn = { GRID_X, GRID_Y + ROWS * CELL_H + 50, 160, 40 };
        Rectangle new_btn = { GRID_X + 180, GRID_Y + ROWS * CELL_H + 50, 160, 40 };

        DrawRectangleRoundedLines(chk_btn, 0.2f, 4, BORDER);
        draw_centered_text("Check", chk_btn, 14, TEXT_PRI);

        DrawRectangleRoundedLines(new_btn, 0.2f, 4, BORDER);
        draw_centered_text("New table", new_btn, 14, TEXT_PRI);

        /* Message */
        if (msg[0]) {
            bool good = (strstr(msg, "correct") != NULL);
            Color mc = good ? C_GREEN : C_RED;
            int tw = MeasureText(msg, 13);
            DrawText(msg, (WIN_W - tw) / 2, GRID_Y + ROWS * CELL_H + 104, 13, mc);
        }

        /* Hint */
        DrawText("Tab to move between inputs  |  Enter to check", GRID_X, WIN_H - 28, 11, TEXT_MUT);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}