/*
 * v_clone.h - VI-style modal logic and rendering abstraction (STB-style)
 */

#ifndef V_CLONE_H
#define V_CLONE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define V_KEY_BACKSPACE 127
#define V_KEY_ENTER     10
#define V_KEY_ESC       27
#define V_KEY_UP        1001
#define V_KEY_DOWN      1002
#define V_KEY_LEFT      1003
#define V_KEY_RIGHT     1004
#define V_KEY_CTRL_O    15

typedef enum { V_MODE_NORMAL, V_MODE_INSERT, V_MODE_COMMAND, V_MODE_SEARCH, V_MODE_VISUAL } v_mode_t;

typedef struct {
    v_mode_t mode;
    char command_buffer[256];
    char search_buffer[256];
    int running;
    int pending_d, pending_g, pending_y;
    int screen_rows, screen_cols;
    int row_offset;
    size_t visual_anchor; 
    int insert_return; // Flag para o Ctrl+O retornar ao modo inserção
    void *udata; 
} v_state_t;

void v_init(v_state_t *v);
void v_process_key(v_state_t *v, int c);
void v_render(v_state_t *v);

#ifdef __cplusplus
}
#endif

#endif // V_CLONE_H

#ifdef V_CLONE_IMPLEMENTATION

#include <string.h>
#include <stdio.h>

#define V_EXPAND(key, action) case key: action; break;
#define V(key, action) case key: action; break;

#ifndef V_CUSTOM_GLOBAL
#define V_CUSTOM_GLOBAL(V, v, c)
#endif
#ifndef V_CUSTOM_NORMAL
#define V_CUSTOM_NORMAL(V, v, c)
#endif
#ifndef V_CUSTOM_INSERT
#define V_CUSTOM_INSERT(V, v, c)
#endif
#ifndef V_CUSTOM_CMD
#define V_CUSTOM_CMD(V, v, c)
#endif
#ifndef V_ACTION_CUSTOM
#define V_ACTION_CUSTOM(v, c)
#endif
#ifndef V_ACTION_COMMAND
#define V_ACTION_COMMAND(v, cmd)
#endif
#ifndef V_ACTION_MOVE_LINE
#define V_ACTION_MOVE_LINE(v, dir)
#endif

// --- PRIMITIVAS TERMINAL ---
#ifndef V_TERM_GOTOXY
#define V_TERM_GOTOXY(x, y)
#endif
#ifndef V_TERM_CLEAR
#define V_TERM_CLEAR()
#endif
#ifndef V_TERM_CURSOR_SHOW
#define V_TERM_CURSOR_SHOW(show)
#endif
#ifndef V_CLR_RESET
#define V_CLR_RESET()
#endif
#ifndef V_CLR_LINENUM
#define V_CLR_LINENUM()
#endif
#ifndef V_CLR_STATUS
#define V_CLR_STATUS()
#endif
#ifndef V_CLR_TEXT
#define V_CLR_TEXT()
#endif
#ifndef V_CLR_SELECTION
#define V_CLR_SELECTION()
#endif

// --- PRIMITIVAS EDITOR ---
#ifndef V_ED_GET_CURSOR
#define V_ED_GET_CURSOR(v) 0
#endif
#ifndef V_ED_SET_CURSOR
#define V_ED_SET_CURSOR(v, pos)
#endif
#ifndef V_ED_GET_CHAR
#define V_ED_GET_CHAR(v, pos) '\0'
#endif
#ifndef V_ED_GET_LENGTH
#define V_ED_GET_LENGTH(v) 0
#endif
#ifndef V_ED_DELETE_RANGE
#define V_ED_DELETE_RANGE(v, start, end)
#endif
#ifndef V_ED_INSERT_TEXT
#define V_ED_INSERT_TEXT(v, text)
#endif
#ifndef V_ED_SAVE_SNAPSHOT
#define V_ED_SAVE_SNAPSHOT(v)
#endif
#ifndef V_ED_UNDO
#define V_ED_UNDO(v)
#endif
#ifndef V_ED_FIND_LINE_START
#define V_ED_FIND_LINE_START(v, pos) pos
#endif
#ifndef V_ED_FIND_LINE_END
#define V_ED_FIND_LINE_END(v, pos) pos
#endif
#ifndef V_ED_GET_ROW_COL
#define V_ED_GET_ROW_COL(v, pos, r, c)
#endif
#ifndef V_ED_YANK
#define V_ED_YANK(v, start, end)
#endif
#ifndef V_ED_PASTE
#define V_ED_PASTE(v)
#endif
#ifndef V_ED_SEARCH
#define V_ED_SEARCH(v, query, forward)
#endif

// --- RENDERIZAÇÃO ---

static void v_scroll(v_state_t *v) {
    size_t r, c;
    V_ED_GET_ROW_COL(v, V_ED_GET_CURSOR(v), &r, &c);
    if ((int)r < v->row_offset) v->row_offset = (int)r;
    if ((int)r >= v->row_offset + v->screen_rows - 1) v->row_offset = (int)r - (v->screen_rows - 2);
}

void v_render(v_state_t *v) {
    v_scroll(v);
    V_TERM_CURSOR_SHOW(0);
    V_CLR_TEXT();
    V_TERM_CLEAR();

    int ln_width = 4;
    size_t len = V_ED_GET_LENGTH(v);
    size_t cur_pos = V_ED_GET_CURSOR(v);
    size_t sel_start = (v->visual_anchor < cur_pos) ? v->visual_anchor : cur_pos;
    size_t sel_end = (v->visual_anchor < cur_pos) ? cur_pos : v->visual_anchor;

    int r_ui = 0, c_ui = 0;
    int in_sel = 0;

    for (size_t i = 0; i <= len; i++) {
        int visible = (r_ui >= v->row_offset && r_ui < v->row_offset + v->screen_rows - 1);
        if (visible) {
            if (c_ui == 0) {
                V_TERM_GOTOXY(1, r_ui - v->row_offset + 1);
                V_CLR_TEXT(); V_CLR_LINENUM(); printf("%3d ", r_ui + 1);
                in_sel = 0; V_CLR_TEXT();
            }
            int s = (v->mode == V_MODE_VISUAL && i >= sel_start && i <= sel_end);
            if (s && !in_sel) { V_CLR_SELECTION(); in_sel = 1; }
            else if (!s && in_sel) { V_CLR_TEXT(); in_sel = 0; }
            V_TERM_GOTOXY(c_ui + 1 + ln_width, r_ui - v->row_offset + 1);
            if (i == len) { if (s) putchar(' '); }
            else { char c = V_ED_GET_CHAR(v, i); if (c == '\n') putchar(' '); else putchar(c); }
        }
        if (i < len) { if (V_ED_GET_CHAR(v, i) == '\n') { r_ui++; c_ui = 0; } else c_ui++; } else break;
    }

    V_CLR_STATUS();
    V_TERM_GOTOXY(1, v->screen_rows);
    size_t r, c; V_ED_GET_ROW_COL(v, cur_pos, &r, &c);
    const char *ms = (v->mode == V_MODE_NORMAL) ? "-- NORMAL --" : (v->mode == V_MODE_INSERT) ? "-- INSERT --" : (v->mode == V_MODE_SEARCH) ? "-- SEARCH --" : (v->mode == V_MODE_VISUAL) ? "-- VISUAL --" : "-- COMMAND --";
    char st[256]; snprintf(st, 256, " %s | L: %zu, C: %zu ", ms, r + 1, c + 1);
    printf("%s", st);
    for (int i = (int)strlen(st); i < v->screen_cols; i++) putchar(' ');
    V_CLR_RESET();

    if (v->mode == V_MODE_COMMAND || v->mode == V_MODE_SEARCH) {
        V_TERM_GOTOXY(1, v->screen_rows);
        printf("%c%s", (v->mode == V_MODE_COMMAND ? ':' : '/'), (v->mode == V_MODE_COMMAND ? v->command_buffer : v->search_buffer));
    } else {
        V_TERM_GOTOXY((int)c + 1 + ln_width, (int)r - v->row_offset + 1);
    }
    V_TERM_CURSOR_SHOW(1);
    fflush(stdout);
}

// --- LÓGICA VI ---
static int v_is_word(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '_'); }
static void v_word_next(v_state_t *v) {
    size_t p = V_ED_GET_CURSOR(v); size_t l = V_ED_GET_LENGTH(v); if (p >= l) return;
    int s_cls = v_is_word(V_ED_GET_CHAR(v, p));
    while (p < l && v_is_word(V_ED_GET_CHAR(v, p)) == s_cls) p++;
    while (p < l && V_ED_GET_CHAR(v, p) <= ' ') p++;
    V_ED_SET_CURSOR(v, p);
}

// --- KEYMAPS ---
#define V_NORMAL_KEYMAP(V, v, c) \
    V('i', { (v)->mode = V_MODE_INSERT; }) \
    V('a', { V_ED_SET_CURSOR(v, V_ED_GET_CURSOR(v) + 1); (v)->mode = V_MODE_INSERT; }) \
    V('v', { (v)->mode = V_MODE_VISUAL; (v)->visual_anchor = V_ED_GET_CURSOR(v); }) \
    V('o', { V_ED_SAVE_SNAPSHOT(v); V_ED_SET_CURSOR(v, V_ED_FIND_LINE_END(v, V_ED_GET_CURSOR(v))); V_ED_INSERT_TEXT(v, "\n"); (v)->mode = V_MODE_INSERT; }) \
    V('O', { V_ED_SAVE_SNAPSHOT(v); size_t s = V_ED_FIND_LINE_START(v, V_ED_GET_CURSOR(v)); V_ED_SET_CURSOR(v, s); V_ED_INSERT_TEXT(v, "\n"); V_ED_SET_CURSOR(v, s); (v)->mode = V_MODE_INSERT; }) \
    V(':', { (v)->mode = V_MODE_COMMAND; (v)->command_buffer[0] = '\0'; }) \
    V('/', { (v)->mode = V_MODE_SEARCH; (v)->search_buffer[0] = '\0'; }) \
    V('h', { V_ED_SET_CURSOR(v, V_ED_GET_CURSOR(v) > 0 ? V_ED_GET_CURSOR(v) - 1 : 0); }) \
    V('l', { V_ED_SET_CURSOR(v, V_ED_GET_CURSOR(v) + 1); }) \
    V('j', { V_ACTION_MOVE_LINE(v, 1); }) \
    V('k', { V_ACTION_MOVE_LINE(v, -1); }) \
    V('u', { V_ED_UNDO(v); }) \
    V('x', { V_ED_SAVE_SNAPSHOT(v); V_ED_DELETE_RANGE(v, V_ED_GET_CURSOR(v), V_ED_GET_CURSOR(v) + 1); }) \
    V('d', { (v)->pending_d = 1; return; }) \
    V('y', { (v)->pending_y = 1; return; }) \
    V('g', { (v)->pending_g = 1; return; }) \
    V('w', { v_word_next(v); }) \
    V('p', { V_ED_PASTE(v); }) \
    V('0', { V_ED_SET_CURSOR(v, V_ED_FIND_LINE_START(v, V_ED_GET_CURSOR(v))); }) \
    V('$', { V_ED_SET_CURSOR(v, V_ED_FIND_LINE_END(v, V_ED_GET_CURSOR(v))); }) \
    V(V_KEY_UP,    { V_ACTION_MOVE_LINE(v, -1); }) \
    V(V_KEY_DOWN,  { V_ACTION_MOVE_LINE(v, 1); }) \
    V(V_KEY_LEFT,  { V_ED_SET_CURSOR(v, V_ED_GET_CURSOR(v) > 0 ? V_ED_GET_CURSOR(v) - 1 : 0); }) \
    V(V_KEY_RIGHT, { V_ED_SET_CURSOR(v, V_ED_GET_CURSOR(v) + 1); }) \
    V(V_KEY_CTRL_O, { V_ACTION_CUSTOM(v, V_KEY_CTRL_O); })

#define V_VISUAL_KEYMAP(V, v, c) \
    V('h', { V_ED_SET_CURSOR(v, V_ED_GET_CURSOR(v) > 0 ? V_ED_GET_CURSOR(v) - 1 : 0); }) \
    V('l', { V_ED_SET_CURSOR(v, V_ED_GET_CURSOR(v) + 1); }) \
    V('j', { V_ACTION_MOVE_LINE(v, 1); }) \
    V('k', { V_ACTION_MOVE_LINE(v, -1); }) \
    V('w', { v_word_next(v); }) \
    V('d', { \
        size_t cp = V_ED_GET_CURSOR(v); \
        size_t s = (v->visual_anchor < cp) ? v->visual_anchor : cp; \
        size_t e = (v->visual_anchor < cp) ? cp : v->visual_anchor; \
        V_ED_SAVE_SNAPSHOT(v); V_ED_DELETE_RANGE(v, s, e + 1); (v)->mode = V_MODE_NORMAL; \
    }) \
    V('y', { \
        size_t cp = V_ED_GET_CURSOR(v); \
        size_t s = (v->visual_anchor < cp) ? v->visual_anchor : cp; \
        size_t e = (v->visual_anchor < cp) ? cp : v->visual_anchor; \
        V_ED_YANK(v, s, e + 1); (v)->mode = V_MODE_NORMAL; \
    })

// --- PROCESSADORES ---
#ifndef V_PROCESS_NORMAL
#define V_PROCESS_NORMAL(v, c) \
    do { \
        if ((v)->pending_d && c == 'd') { \
            size_t p = V_ED_GET_CURSOR(v); V_ED_SAVE_SNAPSHOT(v); \
            V_ED_DELETE_RANGE(v, V_ED_FIND_LINE_START(v, p), V_ED_FIND_LINE_END(v, p) + 1); \
            (v)->pending_d = 0; if ((v)->insert_return) (v)->mode = V_MODE_INSERT; return; \
        } \
        if ((v)->pending_y && c == 'y') { \
            size_t p = V_ED_GET_CURSOR(v); \
            V_ED_YANK(v, V_ED_FIND_LINE_START(v, p), V_ED_FIND_LINE_END(v, p) + 1); \
            (v)->pending_y = 0; if ((v)->insert_return) (v)->mode = V_MODE_INSERT; return; \
        } \
        if ((v)->pending_g && c == 'g') { V_ED_SET_CURSOR(v, 0); (v)->pending_g = 0; if ((v)->insert_return) (v)->mode = V_MODE_INSERT; return; } \
        int pt = (c == 'd' || c == 'y' || c == 'g'); \
        if (!pt) { (v)->pending_d = (v)->pending_g = (v)->pending_y = 0; } \
        switch (c) { \
            V_CUSTOM_NORMAL(V_EXPAND, v, c) \
            V_NORMAL_KEYMAP(V_EXPAND, v, c) \
            default: if (!pt) V_ACTION_CUSTOM(v, c); break; \
        } \
        if (!pt && (v)->insert_return) { (v)->insert_return = 0; (v)->mode = V_MODE_INSERT; } \
    } while(0)
#endif

#ifndef V_PROCESS_VISUAL
#define V_PROCESS_VISUAL(v, c) \
    switch (c) { V_CUSTOM_NORMAL(V_EXPAND, v, c) V_VISUAL_KEYMAP(V_EXPAND, v, c) }
#endif

#ifndef V_PROCESS_INSERT
#define V_PROCESS_INSERT(v, c) \
    switch (c) { \
        V_CUSTOM_INSERT(V_EXPAND, v, c) \
        V(V_KEY_CTRL_O, { (v)->mode = V_MODE_NORMAL; (v)->insert_return = 1; }) \
        V(V_KEY_BACKSPACE, { size_t p = V_ED_GET_CURSOR(v); if (p > 0) V_ED_DELETE_RANGE(v, p-1, p); }) \
        V(V_KEY_ENTER,     { V_ED_SAVE_SNAPSHOT(v); V_ED_INSERT_TEXT(v, "\n"); }) \
        default: if (c < 1000) { char s[2] = {(char)c, 0}; V_ED_INSERT_TEXT(v, s); } break; \
    }
#endif

#ifndef V_PROCESS_COMMAND
#define V_PROCESS_COMMAND(v, c) \
    switch (c) { \
        V_CUSTOM_CMD(V_EXPAND, v, c) \
        V(V_KEY_ENTER, { \
            char *b = ((v)->mode == V_MODE_COMMAND) ? (v)->command_buffer : (v)->search_buffer; \
            if ((v)->mode == V_MODE_COMMAND) V_ACTION_COMMAND(v, b); \
            else V_ED_SEARCH(v, b, 1); \
            (v)->mode = V_MODE_NORMAL; \
        }) \
        V(V_KEY_BACKSPACE, { \
            char *b = ((v)->mode == V_MODE_COMMAND) ? (v)->command_buffer : (v)->search_buffer; \
            size_t l = strlen(b); if (l > 0) b[l-1] = '\0'; \
        }) \
        default: { \
            char *b = ((v)->mode == V_MODE_COMMAND) ? (v)->command_buffer : (v)->search_buffer; \
            size_t l = strlen(b); if (c < 1000 && l < 255) { b[l] = (char)c; b[l+1] = '\0'; } \
            break; \
        } \
    }
#endif

void v_init(v_state_t *v) { memset(v, 0, sizeof(v_state_t)); v->mode = V_MODE_NORMAL; v->running = 1; }

void v_process_key(v_state_t *v, int c) {
    if (c == V_KEY_ESC) {
        (v)->mode = V_MODE_NORMAL; (v)->command_buffer[0] = (v)->search_buffer[0] = '\0';
        (v)->pending_d = (v)->pending_g = (v)->pending_y = (v)->insert_return = 0; return;
    }
    switch (c) { V_CUSTOM_GLOBAL(V_EXPAND, v, c) }
    if (v->mode == V_MODE_NORMAL) { V_PROCESS_NORMAL(v, c); }
    else if (v->mode == V_MODE_VISUAL) { V_PROCESS_VISUAL(v, c); }
    else if (v->mode == V_MODE_INSERT) { V_PROCESS_INSERT(v, c); }
    else if (v->mode == V_MODE_COMMAND || v->mode == V_MODE_SEARCH) { V_PROCESS_COMMAND(v, c); }
}

#endif // V_CLONE_IMPLEMENTATION
