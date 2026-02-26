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

typedef enum { MODE_NORMAL, MODE_INSERT, MODE_COMMAND } Mode;

struct {
    editor_t ed;
    Mode mode;
    char command_buffer[256];
    int screen_rows;
    int screen_cols;
    int row_offset;
    char filename[256];
    int running;
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
                           (State.mode == MODE_INSERT) ? "-- INSERT --" : "-- COMMAND --";
    
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
    }
}

void scroll_view() {
    size_t row, col;
    editor_get_row_col(&State.ed, editor_get_cursor(&State.ed), &row, &col);

    if ((int)row < State.row_offset) {
        State.row_offset = (int)row;
    }
    if ((int)row >= State.row_offset + State.screen_rows - 1) {
        State.row_offset = (int)row - (State.screen_rows - 2);
    }
}

void render() {
    scroll_view();
    term_cursor_show(0);
    term_clear();
    term_gotoxy(1, 1);
    
    size_t len = editor_get_length(&State.ed);
    int current_row = 0;
    int current_col = 0;

    for (size_t i = 0; i < len; i++) {
        char c = editor_get_char(&State.ed, i);
        if (current_row >= State.row_offset && current_row < State.row_offset + State.screen_rows - 1) {
            term_gotoxy(current_col + 1, current_row - State.row_offset + 1);
            if (c != '\n') {
                putchar(c);
            }
        }
        if (c == '\n') {
            current_row++;
            current_col = 0;
        } else {
            current_col++;
        }
    }

    draw_status_bar();
    draw_command_line();

    size_t r, c;
    editor_get_row_col(&State.ed, editor_get_cursor(&State.ed), &r, &c);
    if (State.mode != MODE_COMMAND) {
        term_gotoxy((int)c + 1, (int)r - State.row_offset + 1);
    } else {
        term_gotoxy((int)strlen(State.command_buffer) + 2, State.screen_rows);
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
        strcpy(State.filename, State.command_buffer + 2);
        editor_save_file(&State.ed, State.filename);
    }
    State.mode = MODE_NORMAL;
    State.command_buffer[0] = '\0';
}

void process_input() {
    char c;
    if (read(STDIN_FILENO, &c, 1) <= 0) return;

    if (c == 27) { // ESC
        struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
        if (poll(&pfd, 1, 50) > 0) { // Tem mais caracteres? (Provavelmente uma sequência de escape)
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
        }
        return;
    }

    if (State.mode == MODE_NORMAL) {
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
                editor_move_to_line_end(&State.ed);
                editor_insert_char(&State.ed, '\n');
                State.mode = MODE_INSERT;
                break;
            case ':': State.mode = MODE_COMMAND; break;
            case 'h': editor_move_cursor_relative(&State.ed, -1); break;
            case 'l': editor_move_cursor_relative(&State.ed, 1); break;
            case 'j': editor_move_down(&State.ed); break;
            case 'k': editor_move_up(&State.ed); break;
            case 'x': editor_delete(&State.ed); break;
            case '0': editor_move_to_line_start(&State.ed); break;
            case '$': editor_move_to_line_end(&State.ed); break;
        }
    } else if (State.mode == MODE_INSERT) {
        if (c == 127 || c == 8) {
            editor_backspace(&State.ed);
        } else if (c == 13 || c == 10) {
            editor_insert_char(&State.ed, '\n');
        } else {
            editor_insert_char(&State.ed, c);
        }
    } else if (State.mode == MODE_COMMAND) {
        if (c == 13 || c == 10) {
            handle_command();
        } else if (c == 127 || c == 8) {
            size_t blen = strlen(State.command_buffer);
            if (blen > 0) State.command_buffer[blen - 1] = '\0';
        } else {
            size_t blen = strlen(State.command_buffer);
            if (blen < sizeof(State.command_buffer) - 1) {
                State.command_buffer[blen] = c;
                State.command_buffer[blen+1] = '\0';
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

    if (argc > 1) {
        strncpy(State.filename, argv[1], 255);
        if (!editor_load_file(&State.ed, State.filename)) {
            editor_init(&State.ed, 1024);
        }
    } else {
        editor_init(&State.ed, 1024);
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
