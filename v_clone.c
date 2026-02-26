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

// Key codes
#define KEY_ESC        27
#define KEY_ENTER      13
#define KEY_ENTER_LF   10
#define KEY_BACKSPACE  127
#define KEY_BACKSPACE_ALT 8

// Buffer and UI sizes
#define COMMAND_BUF_SIZE 256
#define SEARCH_BUF_SIZE  256
#define FILENAME_SIZE    256
#define INITIAL_ED_CAP   1024
#define POLL_TIMEOUT     50

// UI Constants
#define DEFAULT_LINE_NUM_WIDTH 4
#define EXTENDED_LINE_NUM_WIDTH 5
#define LINE_NUM_THRESHOLD 999
#define STATUS_BAR_HEIGHT 1

typedef enum { MODE_NORMAL, MODE_INSERT, MODE_COMMAND, MODE_SEARCH } Mode;

struct {
    editor_t ed;
    Mode mode;
    char command_buffer[COMMAND_BUF_SIZE];
    char search_buffer[SEARCH_BUF_SIZE];
    char *clipboard;
    int screen_rows;
    int screen_cols;
    int row_offset;
    char filename[FILENAME_SIZE];
    int running;
    int pending_d;
    int pending_g;
    int pending_y;
} State;

// --- Terminal Raw Mode ---
struct termios orig_termios;
void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    term_cursor_show(1);
    term_clear();
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    term_cursor_show(1);
}

// --- UI Rendering ---
void draw_status_bar() {
    term_set_color(TERM_COLOR_BLACK, TERM_BG_WHITE);
    term_gotoxy(1, State.screen_rows);
    
    size_t row, col;
    editor_get_row_col(&State.ed, editor_get_cursor(&State.ed), &row, &col);
    
    char status[256];
    const char *mode_str = (State.mode == MODE_NORMAL) ? "-- NORMAL --" : 
                           (State.mode == MODE_INSERT) ? "-- INSERT --" :
                           (State.mode == MODE_SEARCH) ? "-- SEARCH --" : "-- COMMAND --";
    
    snprintf(status, sizeof(status), " %s | %s | L: %zu, C: %zu ", 
             mode_str, State.filename[0] ? State.filename : "[No Name]", row + 1, col + 1);
    
    printf("%s", status);
    for (int i = (int)strlen(status); i < State.screen_cols; i++) putchar(' ');
    term_reset();
}

void draw_command_line() {
    if (State.mode == MODE_COMMAND) {
        term_gotoxy(1, State.screen_rows);
        printf(":%s", State.command_buffer);
        for (int i = (int)strlen(State.command_buffer) + 1; i < State.screen_cols; i++) putchar(' ');
    } else if (State.mode == MODE_SEARCH) {
        term_gotoxy(1, State.screen_rows);
        printf("/%s", State.search_buffer);
        for (int i = (int)strlen(State.search_buffer) + 1; i < State.screen_cols; i++) putchar(' ');
    }
}

void scroll_view() {
    size_t row, col;
    editor_get_row_col(&State.ed, editor_get_cursor(&State.ed), &row, &col);

    if ((int)row < State.row_offset) {
        State.row_offset = (int)row;
    }
    if ((int)row >= State.row_offset + State.screen_rows - STATUS_BAR_HEIGHT) {
        State.row_offset = (int)row - (State.screen_rows - STATUS_BAR_HEIGHT - 1);
    }
}

void render() {
    scroll_view();
    term_cursor_show(0);
    term_clear();
    
    int line_num_width = DEFAULT_LINE_NUM_WIDTH;
    int total_lines = editor_count_lines(&State.ed);
    if (total_lines > LINE_NUM_THRESHOLD) line_num_width = EXTENDED_LINE_NUM_WIDTH;

    size_t len = editor_get_length(&State.ed);
    int current_row = 0;
    int current_col = 0;

    // Draw first line number if visible
    if (current_row >= State.row_offset && current_row < State.row_offset + State.screen_rows - STATUS_BAR_HEIGHT) {
        term_gotoxy(1, current_row - State.row_offset + 1);
        term_set_color(TERM_COLOR_BRIGHT_BLACK, TERM_BG_BLACK);
        printf("%*d ", line_num_width - 1, current_row + 1);
        term_reset();
    }

    for (size_t i = 0; i < len; i++) {
        char c = editor_get_char(&State.ed, i);
        if (current_row >= State.row_offset && current_row < State.row_offset + State.screen_rows - STATUS_BAR_HEIGHT) {
            term_gotoxy(current_col + 1 + line_num_width, current_row - State.row_offset + 1);
            if (c != '\n') {
                putchar(c);
            }
        }
        if (c == '\n') {
            current_row++;
            current_col = 0;
            if (current_row >= State.row_offset && current_row < State.row_offset + State.screen_rows - STATUS_BAR_HEIGHT) {
                term_gotoxy(1, current_row - State.row_offset + 1);
                term_set_color(TERM_COLOR_BRIGHT_BLACK, TERM_BG_BLACK);
                printf("%*d ", line_num_width - 1, current_row + 1);
                term_reset();
            }
        } else {
            current_col++;
        }
    }

    draw_status_bar();
    draw_command_line();

    size_t r, c;
    editor_get_row_col(&State.ed, editor_get_cursor(&State.ed), &r, &c);
    if (State.mode == MODE_NORMAL || State.mode == MODE_INSERT) {
        term_gotoxy((int)c + 1 + line_num_width, (int)r - State.row_offset + 1);
    } else if (State.mode == MODE_COMMAND) {
        term_gotoxy((int)strlen(State.command_buffer) + 2, State.screen_rows);
    } else if (State.mode == MODE_SEARCH) {
        term_gotoxy((int)strlen(State.search_buffer) + 2, State.screen_rows);
    }
    
    term_cursor_show(1);
    fflush(stdout);
}

void handle_command() {
    if (strcmp(State.command_buffer, "q") == 0) State.running = 0;
    else if (strcmp(State.command_buffer, "w") == 0) {
        if (State.filename[0]) editor_save_file(&State.ed, State.filename);
    }
    else if (strncmp(State.command_buffer, "w ", 2) == 0) {
        strncpy(State.filename, State.command_buffer + 2, FILENAME_SIZE - 1);
        editor_save_file(&State.ed, State.filename);
    }
    State.mode = MODE_NORMAL;
    State.command_buffer[0] = '\0';
}

void process_input() {
    char c;
    if (read(STDIN_FILENO, &c, 1) <= 0) return;

    if (c == KEY_ESC) { // ESC
        struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
        if (poll(&pfd, 1, POLL_TIMEOUT) > 0) { // Tem mais caracteres?
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) == 0) return;
            if (read(STDIN_FILENO, &seq[1], 1) == 0) return;

            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'A': editor_move_up(&State.ed); break;
                    case 'B': editor_move_down(&State.ed); break;
                    case 'C': editor_move_cursor_relative(&State.ed, 1); break;
                    case 'D': editor_move_cursor_relative(&State.ed, -1); break;
                }
            }
        } else { // ESC isolado
            State.mode = MODE_NORMAL;
            State.command_buffer[0] = '\0';
            State.search_buffer[0] = '\0';
            State.pending_d = 0;
            State.pending_g = 0;
            State.pending_y = 0;
        }
        return;
    }

    if (State.mode == MODE_NORMAL) {
        if (State.pending_d) {
            State.pending_d = 0;
            if (c == 'd') {
                editor_save_snapshot(&State.ed);
                size_t pos = editor_get_cursor(&State.ed);
                size_t start = editor_find_line_start(&State.ed, pos);
                size_t end = editor_find_line_end(&State.ed, pos);
                size_t length = editor_get_length(&State.ed);
                if (end < length && editor_get_char(&State.ed, end) == '\n') end++;
                else if (start > 0 && editor_get_char(&State.ed, start - 1) == '\n') start--;
                editor_delete_range(&State.ed, start, end);
                editor_move_cursor(&State.ed, start);
                return;
            }
        }
        if (State.pending_y) {
            State.pending_y = 0;
            if (c == 'y') {
                size_t pos = editor_get_cursor(&State.ed);
                size_t start = editor_find_line_start(&State.ed, pos);
                size_t end = editor_find_line_end(&State.ed, pos);
                size_t length = editor_get_length(&State.ed);
                if (end < length && editor_get_char(&State.ed, end) == '\n') end++;
                if (State.clipboard) free(State.clipboard);
                State.clipboard = editor_get_range(&State.ed, start, end);
                return;
            }
        }
        if (State.pending_g) {
            State.pending_g = 0;
            if (c == 'g') {
                editor_move_cursor(&State.ed, 0);
                return;
            }
        }

        switch (c) {
            case 'i': State.mode = MODE_INSERT; break;
            case 'a': 
                editor_move_cursor_relative(&State.ed, 1);
                State.mode = MODE_INSERT; 
                break;
            case 'A':
                editor_move_to_line_end(&State.ed);
                State.mode = MODE_INSERT;
                break;
            case 'I':
                editor_move_to_line_start(&State.ed);
                State.mode = MODE_INSERT;
                break;
            case 'o':
                editor_save_snapshot(&State.ed);
                editor_move_to_line_end(&State.ed);
                editor_insert_char(&State.ed, '\n');
                State.mode = MODE_INSERT;
                break;
            case ':': State.mode = MODE_COMMAND; break;
            case '/': State.mode = MODE_SEARCH; break;
            case 'h': editor_move_cursor_relative(&State.ed, -1); break;
            case 'l': editor_move_cursor_relative(&State.ed, 1); break;
            case 'j': editor_move_down(&State.ed); break;
            case 'k': editor_move_up(&State.ed); break;
            case 'w': editor_move_word_next(&State.ed); break;
            case 'e': editor_move_word_end(&State.ed); break;
            case 'b': editor_move_word_prev(&State.ed); break;
            case 'x': 
                editor_save_snapshot(&State.ed);
                editor_delete(&State.ed); 
                break;
            case 'd': State.pending_d = 1; break;
            case 'y': State.pending_y = 1; break;
            case 'p':
                if (State.clipboard) {
                    editor_save_snapshot(&State.ed);
                    editor_insert_text(&State.ed, State.clipboard);
                }
                break;
            case 'n': {
                if (State.search_buffer[0] != '\0') {
                    int pos = editor_find(&State.ed, State.search_buffer, editor_get_cursor(&State.ed) + 1);
                    if (pos == -1) pos = editor_find(&State.ed, State.search_buffer, 0);
                    if (pos != -1) editor_move_cursor(&State.ed, pos);
                }
                break;
            }
            case 'N': {
                if (State.search_buffer[0] != '\0') {
                    // Busca reversa é mais complexa no editor atual, 
                    // por enquanto vamos buscar a última ocorrência antes da atual.
                    size_t current = editor_get_cursor(&State.ed);
                    int last_pos = -1;
                    int find_pos = editor_find(&State.ed, State.search_buffer, 0);
                    while (find_pos != -1 && (size_t)find_pos < current) {
                        last_pos = find_pos;
                        find_pos = editor_find(&State.ed, State.search_buffer, find_pos + 1);
                    }
                    if (last_pos == -1) { // Loop para o fim do arquivo
                        find_pos = editor_find(&State.ed, State.search_buffer, current + 1);
                        while (find_pos != -1) {
                            last_pos = find_pos;
                            find_pos = editor_find(&State.ed, State.search_buffer, find_pos + 1);
                        }
                    }
                    if (last_pos != -1) editor_move_cursor(&State.ed, last_pos);
                }
                break;
            }
            case 'u': editor_undo(&State.ed); break;
            case 'g': State.pending_g = 1; break;
            case 'G': editor_move_cursor(&State.ed, editor_get_length(&State.ed)); break;
            case '0': editor_move_to_line_start(&State.ed); break;
            case '$': editor_move_to_line_end(&State.ed); break;
        }
    } else if (State.mode == MODE_INSERT) {
        if (c == KEY_BACKSPACE || c == KEY_BACKSPACE_ALT) {
            editor_backspace(&State.ed);
        } else if (c == KEY_ENTER || c == KEY_ENTER_LF) {
            editor_save_snapshot(&State.ed);
            editor_insert_char(&State.ed, '\n');
        } else {
            editor_insert_char(&State.ed, c);
        }
    } else if (State.mode == MODE_COMMAND) {
        if (c == KEY_ENTER || c == KEY_ENTER_LF) {
            handle_command();
        } else if (c == KEY_BACKSPACE || c == KEY_BACKSPACE_ALT) {
            size_t blen = strlen(State.command_buffer);
            if (blen > 0) State.command_buffer[blen - 1] = '\0';
        } else {
            size_t blen = strlen(State.command_buffer);
            if (blen < sizeof(State.command_buffer) - 1) {
                State.command_buffer[blen] = c;
                State.command_buffer[blen+1] = '\0';
            }
        }
    } else if (State.mode == MODE_SEARCH) {
        if (c == KEY_ENTER || c == KEY_ENTER_LF) {
            int pos = editor_find(&State.ed, State.search_buffer, editor_get_cursor(&State.ed) + 1);
            if (pos == -1) pos = editor_find(&State.ed, State.search_buffer, 0);
            if (pos != -1) editor_move_cursor(&State.ed, pos);
            State.mode = MODE_NORMAL;
        } else if (c == KEY_BACKSPACE || c == KEY_BACKSPACE_ALT) {
            size_t blen = strlen(State.search_buffer);
            if (blen > 0) State.search_buffer[blen - 1] = '\0';
        } else {
            size_t blen = strlen(State.search_buffer);
            if (blen < sizeof(State.search_buffer) - 1) {
                State.search_buffer[blen] = c;
                State.search_buffer[blen+1] = '\0';
            }
        }
    }
}

int main(int argc, char **argv) {
    State.mode = MODE_NORMAL;
    State.running = 1;
    State.row_offset = 0;
    State.filename[0] = '\0';
    State.command_buffer[0] = '\0';
    State.search_buffer[0] = '\0';
    State.clipboard = NULL;
    State.pending_d = 0;
    State.pending_g = 0;
    State.pending_y = 0;

    if (argc > 1) {
        strncpy(State.filename, argv[1], FILENAME_SIZE - 1);
        if (!editor_load_file(&State.ed, State.filename)) {
            editor_init(&State.ed, INITIAL_ED_CAP);
        }
    } else {
        editor_init(&State.ed, INITIAL_ED_CAP);
    }

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    State.screen_rows = w.ws_row;
    State.screen_cols = w.ws_col;

    enable_raw_mode();

    while (State.running) {
        render();
        process_input();
    }

    return 0;
}
