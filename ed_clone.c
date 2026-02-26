#include <stdio.h>
#define EDITOR_IMPLEMENTATION
#include "editor.h"
#include <stdlib.h>
#include <string.h>

// This is a test
void print_line(editor_t *ed, size_t pos) {
    size_t start = editor_find_line_start(ed, pos);
    size_t end = editor_find_line_end(ed, pos);
    for (size_t i = start; i < end; i++) {
        putchar(editor_get_char(ed, i));
    }
    putchar('\n');
}

void print_all(editor_t *ed) {
    char *s = editor_to_string(ed);
    if (s) {
        printf("%s", s);
        if (s[0] != '\0' && s[strlen(s)-1] != '\n') putchar('\n');
        free(s);
    }
}

void input_mode(editor_t *ed) {
    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, stdin) != -1) {
        if (strcmp(line, ".\n") == 0) break;
        editor_insert_text(ed, line);
    }
    free(line);
}

int main(int argc, char **argv) {
    editor_t ed;
    
    if (argc > 1) {
        if (editor_load_file(&ed, argv[1])) {
            printf("%zu\n", editor_get_length(&ed));
        } else {
            editor_init(&ed, 1024);
        }
    } else {
        editor_init(&ed, 1024);
    }

    char *cmd = NULL;
    size_t cmd_len = 0;

    while (1) {
        if (getline(&cmd, &cmd_len, stdin) == -1) break;
        
        if (strcmp(cmd, "q\n") == 0) break;
        
        if (strcmp(cmd, "a\n") == 0) {
            editor_move_to_line_end(&ed);
            if (editor_get_length(&ed) > 0) editor_insert_char(&ed, '\n');
            input_mode(&ed);
        } else if (strcmp(cmd, "i\n") == 0) {
            editor_move_to_line_start(&ed);
            input_mode(&ed);
        } else if (strcmp(cmd, "p\n") == 0) {
            print_line(&ed, editor_get_cursor(&ed));
        } else if (strcmp(cmd, ",p\n") == 0) {
            print_all(&ed);
        } else if (strcmp(cmd, "d\n") == 0) {
            size_t start = editor_find_line_start(&ed, editor_get_cursor(&ed));
            size_t end = editor_find_line_end(&ed, editor_get_cursor(&ed));
            size_t len = end - start;
            if (editor_get_char(&ed, end) == '\n') len++;
            
            editor_move_cursor(&ed, start);
            for (size_t i = 0; i < len; i++) editor_delete(&ed);
        } else if (cmd[0] == 'w') {
            char filename[256];
            if (sscanf(cmd, "w %s", filename) == 1) {
                if (editor_save_file(&ed, filename)) {
                    printf("%zu\n", editor_get_length(&ed));
                }
            }
        } else if (cmd[0] >= '0' && cmd[0] <= '9') {
            int target_row = atoi(cmd);
            editor_move_cursor(&ed, 0);
            for (int i = 0; i < target_row; i++) {
                editor_move_down(&ed);
            }
            print_line(&ed, editor_get_cursor(&ed));
        } else {
            if (strlen(cmd) > 1) printf("?\n");
        }
    }

    free(cmd);
    editor_free(&ed);
    return 0;
}