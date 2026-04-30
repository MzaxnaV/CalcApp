#include "calcapp.h"

// ── State ─────────────────────────────────────────────────────────────────────

static int    grid[ROWS][COLS];
static Input  inputs[INPUTS];
static char   lap_inputs[INPUTS][12]; // one lap time per sum input
static int    focus      = 0;         // 0-7: which input pair is active
static bool   focus_lap  = false;     // false = sum field, true = lap field
static bool   done       = false;
static char   msg[64]    = "";
static bool   show_analysis = false;
static bool   all_correct   = false;

static std::vector<Session> sessions;
static int    hist_count = 0; // all sessions
static int    hist_timed = 0; // sessions with total lap time > 0
static double hist_best  = -1.0;
static double hist_avg   = -1.0;
static char   sessions_file[512] = "";

// ── Helpers ───────────────────────────────────────────────────────────────────

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

static double session_total(const Session &s) {
    double t = 0.0;
    for (int i = 0; i < INPUTS; i++) t += s.laps[i];
    return t;
}

// ── File I/O ──────────────────────────────────────────────────────────────────

static void init_sessions_file() {
    snprintf(sessions_file, sizeof(sessions_file),
             "%scalcapp_sessions.txt", GetApplicationDirectory());
    FILE *f = fopen(sessions_file, "r");
    if (!f) {
        f = fopen(sessions_file, "w");
        if (f) {
            fprintf(f, "# CalcApp sessions: date  lap1..lap8  correct/8\n");
            fclose(f);
        }
    } else {
        fclose(f);
    }
}

static void load_history() {
    FILE *f = fopen(sessions_file, "r");
    if (!f) return;

    char   line[256];
    double sum = 0.0;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        Session s = {};
        int n = sscanf(line, "%15s %lf %lf %lf %lf %lf %lf %lf %lf %d",
                       s.date,
                       &s.laps[0], &s.laps[1], &s.laps[2], &s.laps[3],
                       &s.laps[4], &s.laps[5], &s.laps[6], &s.laps[7],
                       &s.correct);
        if (n == 10) {
            sessions.push_back(s);
            hist_count++;
            double t = session_total(s);
            if (t > 0.0) {
                sum += t;
                hist_timed++;
                if (hist_best < 0.0 || t < hist_best) hist_best = t;
            }
        }
    }
    fclose(f);
    if (hist_timed > 0) hist_avg = sum / hist_timed;
}

static void save_entry(const double laps[INPUTS], int correct) {
    FILE *f = fopen(sessions_file, "a");
    if (!f) return;

    time_t    now = time(nullptr);
    struct tm *tm_ = localtime(&now);
    char date[16];
    strftime(date, sizeof(date), "%Y-%m-%d", tm_);

    fprintf(f, "%s  %.1f %.1f %.1f %.1f  %.1f %.1f %.1f %.1f  %d\n",
            date,
            laps[0], laps[1], laps[2], laps[3],
            laps[4], laps[5], laps[6], laps[7],
            correct);
    fclose(f);

    Session s = {};
    strncpy(s.date, date, 15);
    for (int i = 0; i < INPUTS; i++) s.laps[i] = laps[i];
    s.correct = correct;
    sessions.push_back(s);

    hist_count++;
    double t = session_total(s);
    if (t > 0.0) {
        double old_sum = hist_avg >= 0.0 ? hist_avg * hist_timed : 0.0;
        hist_timed++;
        hist_avg = (old_sum + t) / hist_timed;
        if (hist_best < 0.0 || t < hist_best) hist_best = t;
    }
}

// ── Layout helpers ────────────────────────────────────────────────────────────

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

// All 8 lap inputs stacked in a column on the right
static Rectangle lap_rect_for(int i) {
    return Rectangle{
        (float)LAP_X,
        (float)(GRID_Y + i * (LAP_H + LAP_GAP)),
        (float)LAP_W, (float)LAP_H
    };
}

// ── Input ─────────────────────────────────────────────────────────────────────

static void handle_lap_input() {
    char *buf = lap_inputs[focus];
    int key = GetCharPressed();
    while (key > 0) {
        int  len = (int)strlen(buf);
        bool ok  = (key >= '0' && key <= '9') ||
                   (key == '.' && !strchr(buf, '.'));
        if (ok && len < (int)sizeof(lap_inputs[0]) - 2) {
            buf[len]     = (char)key;
            buf[len + 1] = '\0';
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
        int len = (int)strlen(buf);
        if (len > 0) buf[len - 1] = '\0';
    }
    if (IsKeyPressed(KEY_TAB)) {
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            if (focus == 0) { focus_lap = false; focus = INPUTS - 1; } // back to last sum
            else            { focus--; }
        } else {
            if (focus == INPUTS - 1) { focus_lap = false; focus = 0; } // done, wrap to first sum
            else                     { focus++; }
        }
    }
    if (IsKeyPressed(KEY_ENTER)) check_answers();
}

void handle_text_input(Input *inp) {
    int key = GetCharPressed();
    while (key > 0) {
        int len = (int)strlen(inp->text);
        if (key >= '0' && key <= '9' && len < 5) {
            inp->text[len]     = (char)key;
            inp->text[len + 1] = '\0';
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
        int len = (int)strlen(inp->text);
        if (len > 0) inp->text[len - 1] = '\0';
    }
    if (IsKeyPressed(KEY_TAB)) {
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            if (focus == 0) { focus_lap = true; focus = INPUTS - 1; } // back to last lap
            else            { focus--; }
        } else {
            if (focus == INPUTS - 1) { focus_lap = true; focus = 0; } // enter lap section
            else                     { focus++; }
        }
    }
    if (IsKeyPressed(KEY_ENTER)) check_answers();
}

void draw_centered_text(const char *txt, Rectangle r, int sz, Color col) {
    int tw = MeasureText(txt, sz);
    DrawText(txt, (int)(r.x + (r.width  - tw) / 2),
                  (int)(r.y + (r.height - sz) / 2), sz, col);
}

// ── Game logic ────────────────────────────────────────────────────────────────

void new_table() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            grid[r][c] = 10 + rand() % 90;

    for (int i = 0; i < ROWS; i++)
        inputs[i] = { {}, row_sum(i), false, false };
    for (int i = 0; i < COLS; i++)
        inputs[ROWS + i] = { {}, col_sum(i), false, false };

    for (int i = 0; i < INPUTS; i++) lap_inputs[i][0] = '\0';

    focus     = 0;
    focus_lap = false;
    done        = false;
    all_correct = false;
    msg[0]      = '\0';
}

void check_answers() {
    for (int i = 0; i < INPUTS; i++) {
        if (inputs[i].text[0] == '\0') {
            snprintf(msg, sizeof(msg), "Fill all 8 answers first.");
            return;
        }
    }
    done = true;

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

    double laps[INPUTS] = {};
    for (int i = 0; i < INPUTS; i++)
        laps[i] = lap_inputs[i][0] ? atof(lap_inputs[i]) : 0.0;

    save_entry(laps, correct);

    double total = 0.0;
    for (int i = 0; i < INPUTS; i++) total += laps[i];

    all_correct = (correct == INPUTS);
    if (all_correct) {
        if      (total > 0.0 && total <= 30.0) snprintf(msg, sizeof(msg), "All correct in %.1fs — Excellent!", total);
        else if (total > 0.0 && total <= 60.0) snprintf(msg, sizeof(msg), "All correct in %.1fs — Good.",      total);
        else if (total > 0.0)                  snprintf(msg, sizeof(msg), "All correct in %.1fs — Keep going.", total);
        else                                   snprintf(msg, sizeof(msg), "All correct!");
    } else {
        snprintf(msg, sizeof(msg), "%d wrong. Corrections shown.", INPUTS - correct);
    }
}

// ── Analysis tab ──────────────────────────────────────────────────────────────

static void draw_line_chart(
    Rectangle                 bounds,
    const char               *title,
    float                     lo,
    float                     hi,
    const std::vector<float> &series,
    Color                     series_col,
    const std::vector<float> *overlay,
    Color                     overlay_col,
    const Theme              &t,
    const std::vector<float> *overlay2    = nullptr,
    Color                     overlay2_col = {})
{
    DrawRectangleRec(bounds, t.surface);
    DrawRectangleLinesEx(bounds, 0.5f, t.border);
    DrawText(title, (int)bounds.x + 6, (int)bounds.y + 5, 11, t.text_mut);

    int n = (int)series.size();
    if (n == 0) {
        draw_centered_text("No data yet", bounds, 13, t.text_mut);
        return;
    }
    if (hi <= lo) hi = lo + 1.0f;

    const float pad_l = 38.0f, pad_r = 10.0f, pad_t = 22.0f, pad_b = 18.0f;
    float px = bounds.x + pad_l;
    float py = bounds.y + pad_t;
    float pw = bounds.width  - pad_l - pad_r;
    float ph = bounds.height - pad_t - pad_b;

    DrawLine((int)px, (int)(py+ph), (int)(px+pw), (int)(py+ph), t.border);
    DrawLine((int)px, (int)py,      (int)px,       (int)(py+ph), t.border);

    float mid_y   = py + ph * 0.5f;
    float mid_val = lo + (hi - lo) * 0.5f;
    DrawLine((int)px, (int)mid_y, (int)(px+pw), (int)mid_y,
             { t.border.r, t.border.g, t.border.b, 100 });

    DrawText(TextFormat("%.0f", hi),      (int)(bounds.x + 2), (int)py,           9, t.text_mut);
    DrawText(TextFormat("%.0f", mid_val), (int)(bounds.x + 2), (int)(mid_y - 5),  9, t.text_mut);
    DrawText(TextFormat("%.0f", lo),      (int)(bounds.x + 2), (int)(py+ph-9),    9, t.text_mut);
    DrawText(TextFormat("n=%d", n),       (int)(px+pw-28),     (int)(py+ph+4),    9, t.text_mut);

    auto xp = [&](int i) {
        return px + (n > 1 ? (float)i / (n - 1) : 0.5f) * pw;
    };
    auto yp = [&](float v) {
        float c = v < lo ? lo : (v > hi ? hi : v);
        return py + ph - (c - lo) / (hi - lo) * ph;
    };

    if (overlay && (int)overlay->size() == n)
        for (int i = 1; i < n; i++)
            DrawLineV({ xp(i-1), yp((*overlay)[i-1]) },
                      { xp(i),   yp((*overlay)[i])   }, overlay_col);

    if (overlay2 && (int)overlay2->size() == n)
        for (int i = 1; i < n; i++)
            DrawLineV({ xp(i-1), yp((*overlay2)[i-1]) },
                      { xp(i),   yp((*overlay2)[i])   }, overlay2_col);

    for (int i = 1; i < n; i++)
        DrawLineV({ xp(i-1), yp(series[i-1]) },
                  { xp(i),   yp(series[i])   }, series_col);
    for (int i = 0; i < n; i++)
        DrawCircle((int)xp(i), (int)yp(series[i]), 3, series_col);
}

static void draw_lap_dist_chart(Rectangle bounds, const Theme &t) {
    DrawRectangleRec(bounds, t.surface);
    DrawRectangleLinesEx(bounds, 0.5f, t.border);
    DrawText("Lap time distribution — last 5 attempts  \xb7  bar = mean",
             (int)bounds.x + 6, (int)bounds.y + 5, 11, t.text_mut);

    // collect up to 5 most recent sessions that have lap data
    std::vector<const Session *> recent;
    for (int i = (int)sessions.size() - 1; i >= 0 && (int)recent.size() < 5; i--)
        if (session_total(sessions[i]) > 0.0)
            recent.push_back(&sessions[i]);
    std::reverse(recent.begin(), recent.end()); // oldest left

    int n = (int)recent.size();
    if (n == 0) { draw_centered_text("No data yet", bounds, 13, t.text_mut); return; }

    float lo = 1e9f, hi = 0.0f;
    for (auto *s : recent)
        for (int i = 0; i < INPUTS; i++)
            if (s->laps[i] > 0.0) {
                lo = std::min(lo, (float)s->laps[i]);
                hi = std::max(hi, (float)s->laps[i]);
            }
    lo = std::max(0.0f, lo * 0.85f);
    hi = hi * 1.15f;
    if (hi <= lo) hi = lo + 1.0f;

    const float pad_l = 38.0f, pad_r = 10.0f, pad_t = 22.0f, pad_b = 20.0f;
    float px = bounds.x + pad_l, py = bounds.y + pad_t;
    float pw = bounds.width - pad_l - pad_r;
    float ph = bounds.height - pad_t - pad_b;

    DrawLine((int)px, (int)(py+ph), (int)(px+pw), (int)(py+ph), t.border);
    DrawLine((int)px, (int)py,      (int)px,       (int)(py+ph), t.border);
    DrawText(TextFormat("%.0f", hi), (int)(bounds.x + 2), (int)py,          9, t.text_mut);
    DrawText(TextFormat("%.0f", lo), (int)(bounds.x + 2), (int)(py+ph - 9), 9, t.text_mut);

    auto yp = [&](float v) {
        float c = v < lo ? lo : (v > hi ? hi : v);
        return py + ph - (c - lo) / (hi - lo) * ph;
    };

    const Color session_cols[5] = {
        { 55,  90, 180, 220 }, { 210, 100,  30, 220 },
        { 59, 140,  59, 220 }, { 150,  60, 150, 220 },
        { 170, 150,  20, 220 },
    };

    float slot_w = pw / (float)(n + 1);
    for (int si = 0; si < n; si++) {
        float       cx  = px + (si + 1) * slot_w;
        const Session *s  = recent[si];
        Color       col = session_cols[si % 5];

        // date label (mm-dd portion)
        const char *lbl = strlen(s->date) >= 10 ? s->date + 5 : s->date;
        DrawText(lbl, (int)(cx - MeasureText(lbl, 8) / 2), (int)(py + ph + 5), 8, t.text_mut);

        float lap_sum = 0.0f; int cnt = 0;
        for (int i = 0; i < INPUTS; i++) {
            if (s->laps[i] <= 0.0) continue;
            float y = yp((float)s->laps[i]);
            DrawCircle((int)cx, (int)y, 3, col);
            lap_sum += (float)s->laps[i]; cnt++;
        }
        if (cnt > 0) {
            float my = yp(lap_sum / cnt);
            DrawLine((int)(cx - 9), (int)my, (int)(cx + 9), (int)my, col);
            DrawLine((int)(cx - 9), (int)my - 1, (int)(cx + 9), (int)my - 1, col);
        }
    }
}

static void draw_analysis_tab(const Theme &t) {
    std::vector<float> totals, avg_overlay, avg_lap, accs;
    double time_sum = 0.0;
    int    time_cnt = 0;

    for (const auto &s : sessions) {
        accs.push_back((float)s.correct / INPUTS * 100.0f);
        double tot = session_total(s);
        if (tot > 0.0) {
            totals.push_back((float)tot);
            avg_lap.push_back((float)(tot / INPUTS));
            time_sum += tot;
            time_cnt++;
            avg_overlay.push_back((float)(time_sum / time_cnt));
        }
    }

    // Summary stats row
    char buf[48];
    const int sx = 30;
    snprintf(buf, sizeof(buf), hist_best >= 0.0 ? "Best  %.1fs" : "Best  —", hist_best);
    DrawText(buf, sx, 68, 11, t.text_pri);
    snprintf(buf, sizeof(buf), hist_avg  >= 0.0 ? "Avg  %.1fs"  : "Avg  —",  hist_avg);
    DrawText(buf, sx + 120, 68, 11, t.text_pri);
    snprintf(buf, sizeof(buf), "%d sessions", hist_count);
    DrawText(buf, sx + 240, 68, 11, t.text_pri);
    if (!accs.empty()) {
        float acc_sum = 0;
        for (float a : accs) acc_sum += a;
        snprintf(buf, sizeof(buf), "Avg accuracy  %.0f%%", acc_sum / accs.size());
        DrawText(buf, sx + 360, 68, 11, t.text_pri);
    }

    constexpr float CX = 30, CW = WIN_W - 60.0f, CH = 138.0f, CGAP = 6.0f;
    constexpr float CY0 = 84.0f;

    // Total time chart (blue = per session, grey = running avg, orange = avg addition time)
    {
        float hi = 60.0f;
        for (float v : totals) if (v * 1.15f > hi) hi = v * 1.15f;
        const Color orange = { 210, 120, 30, 255 };
        Rectangle r = { CX, CY0, CW, CH };
        draw_line_chart(r, "Total time (s)  \xb7  grey=avg total  \xb7  orange=avg addition time",
                        0.0f, hi,
                        totals, t.active_border,
                        &avg_overlay, t.text_mut, t,
                        &avg_lap, orange);
    }

    // Lap time distribution chart
    {
        Rectangle r = { CX, CY0 + CH + CGAP, CW, CH };
        draw_lap_dist_chart(r, t);
    }

    // Accuracy chart
    {
        Rectangle r = { CX, CY0 + (CH + CGAP) * 2, CW, CH };
        draw_line_chart(r, "Accuracy (%)",
                        0.0f, 100.0f,
                        accs, t.green,
                        nullptr, {}, t);
    }

    DrawText(TextFormat("Data: %s", sessions_file), 30, WIN_H - 14, 9, t.text_mut);
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
    srand((unsigned)time(nullptr));
    InitWindow(WIN_W, WIN_H, "4x4 Addition Speed Test");
    SetTargetFPS(60);

    init_sessions_file();
    load_history();
    new_table();

    const Theme t = {
        .bg            = { 248, 247, 244, 255 },
        .surface       = { 237, 236, 232, 255 },
        .border        = { 200, 198, 192, 255 },
        .text_pri      = {  30,  30,  28, 255 },
        .text_mut      = { 120, 118, 112, 255 },
        .active_border = {  55,  90, 180, 255 },
        .correct       = { 234, 243, 222, 255 },
        .wrong         = { 252, 235, 235, 255 },
        .green         = {  59, 109,  17, 255 },
        .red           = { 163,  45,  45, 255 },
    };

    // Tab buttons (top right)
    const Rectangle tab1_rect = { (float)(WIN_W - 178), 8.0f, 80.0f, 22.0f };
    const Rectangle tab2_rect = { (float)(WIN_W - 90),  8.0f, 80.0f, 22.0f };

    while (!WindowShouldClose()) {
        // ── Input ─────────────────────────────────────────────────────────────

        if (!show_analysis && !done) {
            if (focus_lap)
                handle_lap_input();
            else
                handle_text_input(&inputs[focus]);
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mp = GetMousePosition();

            if (CheckCollisionPointRec(mp, tab1_rect)) show_analysis = false;
            if (CheckCollisionPointRec(mp, tab2_rect)) show_analysis = true;

            if (!show_analysis) {
                for (int i = 0; i < INPUTS; i++) {
                    if (CheckCollisionPointRec(mp, (i < ROWS)
                            ? row_input_rect(i) : col_input_rect(i - ROWS)))
                        { focus = i; focus_lap = false; break; }
                    if (CheckCollisionPointRec(mp, lap_rect_for(i)))
                        { focus = i; focus_lap = true; break; }
                }

                Rectangle chk_btn = { (float)GRID_X,       (float)BTN_Y, 160, 40 };
                Rectangle new_btn = { (float)GRID_X + 180, (float)BTN_Y, 160, 40 };
                if (CheckCollisionPointRec(mp, chk_btn) && !done) check_answers();
                if (CheckCollisionPointRec(mp, new_btn))           new_table();
            }
        }

        // ── Draw ──────────────────────────────────────────────────────────────

        BeginDrawing();
        ClearBackground(t.bg);

        // Title
        DrawText("4x4 Addition", GRID_X, 14, 18, t.text_pri);
        DrawText("Speed Test", GRID_X + MeasureText("4x4 Addition", 18) + 8, 14, 18, t.text_mut);

        // Historical stats — row 2, below the tab buttons (tabs occupy y=8-30)
        {
            char buf[32];
            int sx = WIN_W - 220;
            DrawText("Best",     sx,       36, 11, t.text_mut);
            DrawText("Avg",      sx + 72,  36, 11, t.text_mut);
            DrawText("Sessions", sx + 140, 36, 11, t.text_mut);
            snprintf(buf, sizeof(buf), hist_best >= 0.0 ? "%.1fs" : "—", hist_best);
            DrawText(buf, sx, 50, 14, t.text_pri);
            snprintf(buf, sizeof(buf), hist_avg  >= 0.0 ? "%.1fs" : "—", hist_avg);
            DrawText(buf, sx + 72, 50, 14, t.text_pri);
            snprintf(buf, sizeof(buf), "%d", hist_count);
            DrawText(buf, sx + 140, 50, 14, t.text_pri);
        }

        // Tab buttons
        {
            auto draw_tab = [&](Rectangle r, const char *label, bool active_tab) {
                DrawRectangleRec(r, active_tab ? t.active_border : t.surface);
                DrawRectangleLinesEx(r, 1.0f, active_tab ? t.active_border : t.border);
                draw_centered_text(label, r, 12, active_tab ? WHITE : t.text_mut);
            };
            draw_tab(tab1_rect, "Practice", !show_analysis);
            draw_tab(tab2_rect, "Analysis",  show_analysis);
        }

        // ── Practice tab ──────────────────────────────────────────────────────
        if (!show_analysis) {
            // Column / row labels
            {
                char buf[16];
                for (int c = 0; c < COLS; c++) {
                    snprintf(buf, sizeof(buf), "col %d", c + 1);
                    Rectangle r = cell_rect(0, c);
                    r.y = GRID_Y - 22; r.height = 20;
                    draw_centered_text(buf, r, 11, t.text_mut);
                }
                for (int r = 0; r < ROWS; r++) {
                    snprintf(buf, sizeof(buf), "row %d", r + 1);
                    DrawText(buf, GRID_X - 70,
                             (int)(GRID_Y + r * CELL_H + (CELL_H - 11) / 2), 11, t.text_mut);
                }
                // "sum" headers
                DrawText("sum", GRID_X + COLS * CELL_W + 8,           GRID_Y - 22, 11, t.text_mut);
                DrawText("sum", GRID_X - 70, (int)(col_input_rect(0).y + (CELL_H - 11) / 2), 11, t.text_mut);
                DrawText("laps", LAP_X, GRID_Y - 22, 11, t.text_mut);
            }

            // Grid cells
            {
                char buf[16];
                for (int r = 0; r < ROWS; r++) {
                    for (int c = 0; c < COLS; c++) {
                        Rectangle rect = cell_rect(r, c);
                        DrawRectangleRec(rect, t.surface);
                        DrawRectangleLinesEx(rect, 0.5f, t.border);
                        snprintf(buf, sizeof(buf), "%d", grid[r][c]);
                        draw_centered_text(buf, rect, FONT_SZ, t.text_pri);
                    }
                }
            }

            // Sum input boxes
            for (int i = 0; i < INPUTS; i++) {
                Rectangle rect     = (i < ROWS) ? row_input_rect(i) : col_input_rect(i - ROWS);
                bool      focused  = (i == focus && !focus_lap);
                Color     bg       = t.surface;
                Color     border_c = focused ? t.active_border : t.border;
                Color     txt_c    = t.text_pri;

                if (inputs[i].checked) {
                    bg    = inputs[i].is_correct ? t.correct : t.wrong;
                    txt_c = inputs[i].is_correct ? t.green   : t.red;
                    border_c = t.border;
                }

                DrawRectangleRec(rect, bg);
                DrawRectangleLinesEx(rect, (focused && !done) ? 1.5f : 0.5f, border_c);

                char disp[16];
                if (focused && !done && (int)(GetTime() * 2) % 2 == 0)
                    snprintf(disp, sizeof(disp), "%s|", inputs[i].text);
                else
                    snprintf(disp, sizeof(disp), "%s", inputs[i].text[0] ? inputs[i].text : "?");
                draw_centered_text(disp, rect, FONT_SZ,
                    inputs[i].text[0] ? txt_c : t.text_mut);
            }

            // Lap input boxes
            for (int i = 0; i < INPUTS; i++) {
                Rectangle  rect     = lap_rect_for(i);
                bool       focused  = (i == focus && focus_lap);
                Color      border_c = focused ? t.active_border : t.border;

                DrawRectangleRec(rect, t.surface);
                DrawRectangleLinesEx(rect, (focused && !done) ? 1.5f : 0.5f, border_c);

                char disp[16];
                if (focused && !done && (int)(GetTime() * 2) % 2 == 0)
                    snprintf(disp, sizeof(disp), "%s|", lap_inputs[i]);
                else
                    snprintf(disp, sizeof(disp), "%s",
                             lap_inputs[i][0] ? lap_inputs[i] : "—");
                draw_centered_text(disp, rect, 13,
                    lap_inputs[i][0] ? t.text_pri : t.text_mut);
            }

            // Buttons
            {
                Rectangle chk_btn = { (float)GRID_X,       (float)BTN_Y, 160, 40 };
                Rectangle new_btn = { (float)GRID_X + 180, (float)BTN_Y, 160, 40 };
                DrawRectangleRoundedLines(chk_btn, 0.2f, 4, t.border);
                draw_centered_text("Check",     chk_btn, 14, t.text_pri);
                DrawRectangleRoundedLines(new_btn, 0.2f, 4, t.border);
                draw_centered_text("New table", new_btn, 14, t.text_pri);
            }

            // Message
            if (msg[0]) {
                Color mc = all_correct ? t.green : t.red;
                DrawText(msg, (WIN_W - MeasureText(msg, 13)) / 2, MSG_Y, 13, mc);
            }

            DrawText("Tab: cycle sums first, then laps  |  Enter: check",
                     GRID_X, WIN_H - 18, 10, t.text_mut);
        }

        // ── Analysis tab ──────────────────────────────────────────────────────
        if (show_analysis)
            draw_analysis_tab(t);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
