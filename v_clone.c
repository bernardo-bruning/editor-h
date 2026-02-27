#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <poll.h>

#define EDITOR_IMPLEMENTATION
#include "editor.h"

#define TERMINAL_IMPLEMENTATION
#include "terminal.h"

#define FILENAME_SIZE    256
#define INITIAL_ED_CAP   1024
#define POLL_TIMEOUT     50

#include "v_clone.h"

typedef struct State_s {
    editor_t ed;
    v_state_t v;
    char *clipboard;
    char filename[FILENAME_SIZE];
} State_t;

State_t State;
struct termios orig_termios;

// --- Cores Pastéis ---
#define CLR_PASTEL_PINK   255, 179, 186
#define CLR_PASTEL_GREEN  186, 255, 201
#define CLR_PASTEL_BLUE   186, 225, 255
#define CLR_PASTEL_YELLOW 255, 255, 186
#define CLR_PASTEL_PURPLE 221, 160, 221
#define CLR_SOFT_WHITE    240, 240, 240
#define CLR_SELECTION_BG  60, 60, 60 // Cinza escuro para destaque

// --- Macros de Cor Robustas ---
// V_CLR_TEXT agora reseta o fundo para evitar vazamentos
#define V_CLR_RESET()     term_reset()
#define V_CLR_TEXT()      do { term_reset(); term_fg_rgb(CLR_SOFT_WHITE); } while(0)
#define V_CLR_LINENUM()   term_fg_rgb(CLR_PASTEL_PURPLE)
#define V_CLR_STATUS()    do { term_fg_rgb(40, 40, 40); term_bg_rgb(CLR_PASTEL_BLUE); } while(0)
#define V_CLR_SELECTION() term_bg_rgb(CLR_SELECTION_BG)

// --- Primitivas de Terminal ---
#define V_TERM_GOTOXY(x, y)      term_gotoxy(x, y)
#define V_TERM_CLEAR()           term_clear()
#define V_TERM_CURSOR_SHOW(s)    term_cursor_show(s)

// --- Primitivas de Editor ---
#define V_ED_GET_CURSOR(v)          editor_get_cursor(&((State_t*)(v)->udata)->ed)
#define V_ED_SET_CURSOR(v, pos)     editor_move_cursor(&((State_t*)(v)->udata)->ed, pos)
#define V_ED_GET_CHAR(v, pos)       editor_get_char(&((State_t*)(v)->udata)->ed, pos)
#define V_ED_GET_LENGTH(v)          editor_get_length(&((State_t*)(v)->udata)->ed)
#define V_ED_DELETE_RANGE(v, s, e)  editor_delete_range(&((State_t*)(v)->udata)->ed, s, e)
#define V_ED_INSERT_TEXT(v, txt)    editor_insert_text(&((State_t*)(v)->udata)->ed, txt)
#define V_ED_SAVE_SNAPSHOT(v)       editor_save_snapshot(&((State_t*)(v)->udata)->ed)
#define V_ED_UNDO(v)                editor_undo(&((State_t*)(v)->udata)->ed)
#define V_ED_FIND_LINE_START(v, p)  editor_find_line_start(&((State_t*)(v)->udata)->ed, p)
#define V_ED_FIND_LINE_END(v, p)    editor_find_line_end(&((State_t*)(v)->udata)->ed, p)
#define V_ED_GET_ROW_COL(v, p, r, c) editor_get_row_col(&((State_t*)(v)->udata)->ed, p, r, c)

#define V_ED_YANK(v, s, e) do { \
    State_t *s_ptr = (State_t*)(v)->udata; \
    if (s_ptr->clipboard) free(s_ptr->clipboard); \
    s_ptr->clipboard = editor_get_range(&s_ptr->ed, s, e); \
} while(0)

#define V_ED_PASTE(v) do { \
    State_t *s_ptr = (State_t*)(v)->udata; \
    if (s_ptr->clipboard) { editor_save_snapshot(&s_ptr->ed); editor_insert_text(&s_ptr->ed, s_ptr->clipboard); } \
} while(0)

#define V_ED_SEARCH(v, q, fwd) do { \
    State_t *s_ptr = (State_t*)(v)->udata; \
    int pos = editor_find(&s_ptr->ed, q, editor_get_cursor(&s_ptr->ed) + 1); \
    if (pos == -1) pos = editor_find(&s_ptr->ed, q, 0); \
    if (pos != -1) editor_move_cursor(&s_ptr->ed, pos); \
} while(0)

#define V_ACTION_MOVE_LINE(v, dir) do { \
    State_t *s_ptr = (State_t*)(v)->udata; \
    if (dir > 0) editor_move_down(&s_ptr->ed); else editor_move_up(&s_ptr->ed); \
} while(0)

#define V_ACTION_COMMAND(v, cmd) do { \
    State_t *s_ptr = (State_t*)(v)->udata; \
    if (strcmp(cmd, "q") == 0) (v)->running = 0; \
    else if (strcmp(cmd, "w") == 0) { if (s_ptr->filename[0]) editor_save_file(&s_ptr->ed, s_ptr->filename); } \
    else if (strncmp(cmd, "w ", 2) == 0) { strncpy(s_ptr->filename, cmd + 2, FILENAME_SIZE - 1); editor_save_file(&s_ptr->ed, s_ptr->filename); } \
} while(0)

#define V_CLONE_IMPLEMENTATION
#include "v_clone.h"

void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); term_cursor_show(1); term_clear(); }
void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios); atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(int argc, char **argv) {
    v_init(&State.v);
    State.v.udata = &State;
    if (argc > 1) {
        strncpy(State.filename, argv[1], FILENAME_SIZE - 1);
        if (!editor_load_file(&State.ed, State.filename)) editor_init(&State.ed, INITIAL_ED_CAP);
    } else editor_init(&State.ed, INITIAL_ED_CAP);

    struct winsize w; ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    State.v.screen_rows = w.ws_row; State.v.screen_cols = w.ws_col;

    enable_raw_mode();
    while (State.v.running) {
        v_render(&State.v);
        char c;
        if (read(STDIN_FILENO, &c, 1) > 0) {
            int key = (unsigned char)c;
            if (key == 127 || key == 8) key = V_KEY_BACKSPACE;
            if (key == 13 || key == 10) key = V_KEY_ENTER;
            v_process_key(&State.v, key);
        }
    }
    return 0;
}
