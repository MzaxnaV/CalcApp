#include "calcapp.h"

static int    grid[ROWS][COLS];
static Input  inputs[INPUTS];
static int    active   = 0;
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

void new_table() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            grid[r][c] = 10 + rand() % 90;

    for (int i = 0; i < ROWS; i++) {
        inputs[i] = { {}, row_sum(i), false, false };
    }
    for (int i = 0; i < COLS; i++) {
        inputs[ROWS + i] = { {}, col_sum(i), false, false };
    }

    active  = 0;
    timing  = false;
    done    = false;
    elapsed = 0.0;
    msg[0]  = '\0';
}

void check_answers() {
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
        if (inputs[i].is_correct)
            correct++;
        else
            snprintf(inputs[i].text, sizeof(inputs[i].text), "%d", inputs[i].correct_val);
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

Rectangle cell_rect(int r, int c) {
    return Rectangle{
        (float)(GRID_X + c * CELL_W),
        (float)(GRID_Y + r * CELL_H),
        CELL_W, CELL_H
    };
}

Rectangle row_input_rect(int r) {
    return Rectangle{
        (float)(GRID_X + COLS * CELL_W),
        (float)(GRID_Y + r * CELL_H),
        CELL_W + 20, CELL_H
    };
}

Rectangle col_input_rect(int c) {
    return Rectangle{
        (float)(GRID_X + c * CELL_W),
        (float)(GRID_Y + ROWS * CELL_H),
        CELL_W, CELL_H
    };
}

void handle_text_input(Input *inp) {
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
    if (IsKeyPressed(KEY_TAB))
        active = (active + (IsKeyDown(KEY_LEFT_SHIFT) ? INPUTS - 1 : 1)) % INPUTS;
    if (IsKeyPressed(KEY_ENTER))
        check_answers();
}

void draw_centered_text(const char *txt, Rectangle r, int sz, Color col) {
    int tw = MeasureText(txt, sz);
    DrawText(txt, (int)(r.x + (r.width  - tw) / 2),
                  (int)(r.y + (r.height - sz) / 2), sz, col);
}

int main() {
    srand((unsigned)time(nullptr));
    InitWindow(WIN_W, WIN_H, "4x4 Addition Speed Test");
    SetTargetFPS(60);
    new_table();

    const Theme t = {
        .bg           = { 248, 247, 244, 255 },
        .surface      = { 237, 236, 232, 255 },
        .border       = { 200, 198, 192, 255 },
        .text_pri     = {  30,  30,  28, 255 },
        .text_mut     = { 120, 118, 112, 255 },
        .active_border= {  55,  90, 180, 255 },
        .correct      = { 234, 243, 222, 255 },
        .wrong        = { 252, 235, 235, 255 },
        .green        = {  59, 109,  17, 255 },
        .red          = { 163,  45,  45, 255 },
    };

    while (!WindowShouldClose()) {
        if (!done) handle_text_input(&inputs[active]);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mp = GetMousePosition();
            for (int r = 0; r < ROWS; r++) {
                if (CheckCollisionPointRec(mp, row_input_rect(r))) { active = r; break; }
            }
            for (int c = 0; c < COLS; c++) {
                if (CheckCollisionPointRec(mp, col_input_rect(c))) { active = ROWS + c; break; }
            }
            Rectangle chk_btn = { GRID_X,       (float)BTN_Y, 160, 40 };
            Rectangle new_btn = { GRID_X + 180, (float)BTN_Y, 160, 40 };
            if (CheckCollisionPointRec(mp, chk_btn) && !done) check_answers();
            if (CheckCollisionPointRec(mp, new_btn))          new_table();
        }

        double display_t = timing ? (GetTime() - start_t) : elapsed;

        BeginDrawing();
        ClearBackground(t.bg);

        // Title
        DrawText("4x4 Addition", GRID_X, 24, 20, t.text_pri);
        DrawText("Speed Test", GRID_X + MeasureText("4x4 Addition", 20) + 8, 24, 20, t.text_mut);

        // Stats
        char buf[32];
        int sx = WIN_W - 240;
        DrawText("Time",     sx,       28, 11, t.text_mut);
        DrawText("Best",     sx + 80,  28, 11, t.text_mut);
        DrawText("Attempts", sx + 160, 28, 11, t.text_mut);

        snprintf(buf, sizeof(buf), "%.1fs", display_t);
        DrawText(buf, sx, 42, 18, t.text_pri);

        snprintf(buf, sizeof(buf), best >= 0.0 ? "%.1fs" : "—", best);
        DrawText(buf, sx + 80, 42, 18, t.text_pri);

        snprintf(buf, sizeof(buf), "%d", attempts);
        DrawText(buf, sx + 160, 42, 18, t.text_pri);

        // Column headers and row labels
        for (int c = 0; c < COLS; c++) {
            snprintf(buf, sizeof(buf), "col %d", c + 1);
            Rectangle r = cell_rect(0, c);
            r.y = GRID_Y - 22; r.height = 20;
            draw_centered_text(buf, r, 11, t.text_mut);
        }
        for (int r = 0; r < ROWS; r++) {
            snprintf(buf, sizeof(buf), "row %d", r + 1);
            DrawText(buf, GRID_X - 70, (int)(GRID_Y + r * CELL_H + (CELL_H - 11) / 2), 11, t.text_mut);
        }
        DrawText("sum", GRID_X + COLS * CELL_W + 8, GRID_Y - 22, 11, t.text_mut);
        {
            Rectangle r = col_input_rect(0);
            DrawText("sum", GRID_X - 70, (int)(r.y + (CELL_H - 11) / 2), 11, t.text_mut);
        }

        // Grid cells
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                Rectangle rect = cell_rect(r, c);
                DrawRectangleRec(rect, t.surface);
                DrawRectangleLinesEx(rect, 0.5f, t.border);
                snprintf(buf, sizeof(buf), "%d", grid[r][c]);
                draw_centered_text(buf, rect, FONT_SZ, t.text_pri);
            }
        }

        // Input boxes
        for (int i = 0; i < INPUTS; i++) {
            Rectangle rect     = (i < ROWS) ? row_input_rect(i) : col_input_rect(i - ROWS);
            Color     bg       = t.surface;
            Color     border_c = t.border;
            Color     txt_c    = t.text_pri;

            if (inputs[i].checked) {
                bg    = inputs[i].is_correct ? t.correct : t.wrong;
                txt_c = inputs[i].is_correct ? t.green   : t.red;
            } else if (i == active) {
                border_c = t.active_border;
            }

            DrawRectangleRec(rect, bg);
            DrawRectangleLinesEx(rect, (i == active && !done) ? 1.5f : 0.5f, border_c);

            char disp[16];
            if (i == active && !done && (int)(GetTime() * 2) % 2 == 0)
                snprintf(disp, sizeof(disp), "%s|", inputs[i].text);
            else
                snprintf(disp, sizeof(disp), "%s", inputs[i].text[0] ? inputs[i].text : "?");

            draw_centered_text(disp, rect, FONT_SZ, inputs[i].text[0] ? txt_c : t.text_mut);
        }

        // Buttons
        Rectangle chk_btn = { GRID_X,       (float)BTN_Y, 160, 40 };
        Rectangle new_btn = { GRID_X + 180, (float)BTN_Y, 160, 40 };
        DrawRectangleRoundedLines(chk_btn, 0.2f, 4, t.border);
        draw_centered_text("Check",     chk_btn, 14, t.text_pri);
        DrawRectangleRoundedLines(new_btn, 0.2f, 4, t.border);
        draw_centered_text("New table", new_btn, 14, t.text_pri);

        // Message
        if (msg[0]) {
            Color mc = strstr(msg, "correct") ? t.green : t.red;
            DrawText(msg, (WIN_W - MeasureText(msg, 13)) / 2, MSG_Y, 13, mc);
        }

        DrawText("Tab to move between inputs  |  Enter to check", GRID_X, WIN_H - 28, 11, t.text_mut);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
