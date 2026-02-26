/*
 * editor.h - STB-style text editing library
 * 
 * To use this library, do:
 * #define EDITOR_IMPLEMENTATION
 * #include "editor.h"
 * 
 * This library provides a gap buffer based text editor core.
 */

#ifndef EDITOR_H
#define EDITOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *buffer;
    size_t capacity;
    size_t gap_start;
    size_t gap_end;
    
    // Undo support (stack of content snapshots + cursor positions)
    char **undo_stack;
    size_t *undo_cursor_stack;
    int undo_top;
    int undo_capacity;
} editor_t;

// Initialize the editor with an initial capacity
void editor_init(editor_t *ed, size_t initial_capacity);

// Salva o estado atual para desfazer futuramente
void editor_save_snapshot(editor_t *ed);

// Desfaz a última alteração
void editor_undo(editor_t *ed);

// Retorna uma string com o conteúdo do intervalo [start, end). O chamador deve liberar a memória.
char* editor_get_range(const editor_t *ed, size_t start, size_t end);

// Conta o número total de linhas
int editor_count_lines(const editor_t *ed);

// Free resources used by the editor
void editor_free(editor_t *ed);

// Insert a character at the current cursor position
void editor_insert_char(editor_t *ed, char c);

// Insert a string at the current cursor position
void editor_insert_text(editor_t *ed, const char *text);

// Delete the character before the cursor (backspace)
void editor_backspace(editor_t *ed);

// Delete the character after the cursor (delete key)
void editor_delete(editor_t *ed);

// Delete a range of characters [start, end)
void editor_delete_range(editor_t *ed, size_t start, size_t end);

// Move cursor to a specific position
void editor_move_cursor(editor_t *ed, size_t pos);

// Move cursor by an offset
void editor_move_cursor_relative(editor_t *ed, int offset);

// Get current cursor position
size_t editor_get_cursor(const editor_t *ed);

// Get total text length
size_t editor_get_length(const editor_t *ed);

// Get a null-terminated string of the entire text. 
// Caller must free the returned pointer.
char* editor_to_string(const editor_t *ed);

// --- File I/O ---

// Load content from a file. Returns 1 on success, 0 on failure.
int editor_load_file(editor_t *ed, const char *filename);

// Save content to a file. Returns 1 on success, 0 on failure.
int editor_save_file(const editor_t *ed, const char *filename);

// Get the character at a specific index (index ignores the gap)
char editor_get_char(const editor_t *ed, size_t index);

// --- Line Utilities ---

// Find the start of the line containing pos
size_t editor_find_line_start(const editor_t *ed, size_t pos);

// Find the end of the line containing pos
size_t editor_find_line_end(const editor_t *ed, size_t pos);

// Get current row and column (0-indexed)
void editor_get_row_col(const editor_t *ed, size_t pos, size_t *row, size_t *col);

// Navigation
void editor_move_up(editor_t *ed);
void editor_move_down(editor_t *ed);
void editor_move_to_line_start(editor_t *ed);
void editor_move_to_line_end(editor_t *ed);
void editor_move_word_next(editor_t *ed);
void editor_move_word_end(editor_t *ed);
void editor_move_word_prev(editor_t *ed);

// Search for a string starting from current cursor position.
// Returns the position of the first occurrence or -1 if not found.
// Does NOT move the cursor.
int editor_find(const editor_t *ed, const char *query, size_t start_pos);

#ifdef __cplusplus
}
#endif

#endif // EDITOR_H

#ifdef EDITOR_IMPLEMENTATION

#ifndef EDITOR_MALLOC
#include <stdlib.h>
#define EDITOR_MALLOC(sz) malloc(sz)
#endif

#ifndef EDITOR_FREE
#include <stdlib.h>
#define EDITOR_FREE(p) free(p)
#endif

#ifndef EDITOR_MEMCPY
#include <string.h>
#define EDITOR_MEMCPY(dst, src, sz) memcpy(dst, src, sz)
#endif

#ifndef EDITOR_MEMMOVE
#include <string.h>
#define EDITOR_MEMMOVE(dst, src, sz) memmove(dst, src, sz)
#endif

#ifndef EDITOR_STRLEN
#include <string.h>
#define EDITOR_STRLEN(s) strlen(s)
#endif

#include <stdio.h>

void editor_init(editor_t *ed, size_t initial_capacity) {
    if (initial_capacity == 0) initial_capacity = 64;
    ed->buffer = (char *)EDITOR_MALLOC(initial_capacity);
    ed->capacity = initial_capacity;
    ed->gap_start = 0;
    ed->gap_end = initial_capacity;
    
    // Undo stack
    ed->undo_capacity = 32;
    ed->undo_stack = (char **)EDITOR_MALLOC(sizeof(char*) * ed->undo_capacity);
    ed->undo_cursor_stack = (size_t *)EDITOR_MALLOC(sizeof(size_t) * ed->undo_capacity);
    ed->undo_top = -1;
}

void editor_free(editor_t *ed) {
    EDITOR_FREE(ed->buffer);
    for (int i = 0; i <= ed->undo_top; i++) EDITOR_FREE(ed->undo_stack[i]);
    EDITOR_FREE(ed->undo_stack);
    EDITOR_FREE(ed->undo_cursor_stack);
    ed->buffer = NULL;
    ed->capacity = 0;
    ed->gap_start = 0;
    ed->gap_end = 0;
}

void editor_save_snapshot(editor_t *ed) {
    if (ed->undo_top + 1 >= ed->undo_capacity) {
        // Remove a base se estiver cheio (LIFO)
        EDITOR_FREE(ed->undo_stack[0]);
        for (int i = 0; i < ed->undo_top; i++) {
            ed->undo_stack[i] = ed->undo_stack[i+1];
            ed->undo_cursor_stack[i] = ed->undo_cursor_stack[i+1];
        }
    } else {
        ed->undo_top++;
    }
    ed->undo_stack[ed->undo_top] = editor_to_string(ed);
    ed->undo_cursor_stack[ed->undo_top] = editor_get_cursor(ed);
}

void editor_undo(editor_t *ed) {
    if (ed->undo_top < 0) return;
    
    char *text = ed->undo_stack[ed->undo_top];
    size_t saved_cursor = ed->undo_cursor_stack[ed->undo_top];
    ed->undo_top--;

    size_t len = EDITOR_STRLEN(text);
    
    EDITOR_FREE(ed->buffer);
    ed->buffer = (char *)EDITOR_MALLOC(len + 64);
    ed->capacity = len + 64;
    EDITOR_MEMCPY(ed->buffer, text, len);
    ed->gap_start = len;
    ed->gap_end = ed->capacity;
    
    // Agora movemos o cursor para a posição salva
    editor_move_cursor(ed, saved_cursor);
    
    EDITOR_FREE(text);
}

char* editor_get_range(const editor_t *ed, size_t start, size_t end) {
    if (start >= end) return NULL;
    size_t len = end - start;
    char *res = (char*)EDITOR_MALLOC(len + 1);
    for (size_t i = 0; i < len; i++) {
        res[i] = editor_get_char(ed, start + i);
    }
    res[len] = '\0';
    return res;
}

int editor_count_lines(const editor_t *ed) {
    int lines = 1;
    size_t len = editor_get_length(ed);
    for (size_t i = 0; i < len; i++) {
        if (editor_get_char(ed, i) == '\n') lines++;
    }
    return lines;
}

static void editor_grow(editor_t *ed, size_t min_extra) {
    size_t current_gap_size = ed->gap_end - ed->gap_start;
    size_t new_capacity = ed->capacity * 2;
    if (new_capacity < ed->capacity + min_extra) {
        new_capacity = ed->capacity + min_extra + 64;
    }

    char *new_buffer = (char *)EDITOR_MALLOC(new_capacity);
    
    // Copy prefix
    EDITOR_MEMCPY(new_buffer, ed->buffer, ed->gap_start);
    
    // Copy suffix to the end of the new buffer
    size_t suffix_len = ed->capacity - ed->gap_end;
    size_t new_gap_end = new_capacity - suffix_len;
    EDITOR_MEMCPY(new_buffer + new_gap_end, ed->buffer + ed->gap_end, suffix_len);
    
    EDITOR_FREE(ed->buffer);
    ed->buffer = new_buffer;
    ed->capacity = new_capacity;
    ed->gap_end = new_gap_end;
}

void editor_move_cursor(editor_t *ed, size_t pos) {
    size_t length = editor_get_length(ed);
    if (pos > length) pos = length;

    if (pos < ed->gap_start) {
        // Move gap left
        size_t dist = ed->gap_start - pos;
        EDITOR_MEMMOVE(ed->buffer + ed->gap_end - dist, ed->buffer + pos, dist);
        ed->gap_start -= dist;
        ed->gap_end -= dist;
    } else if (pos > ed->gap_start) {
        // Move gap right
        size_t dist = pos - ed->gap_start;
        EDITOR_MEMMOVE(ed->buffer + ed->gap_start, ed->buffer + ed->gap_end, dist);
        ed->gap_start += dist;
        ed->gap_end += dist;
    }
}

void editor_move_cursor_relative(editor_t *ed, int offset) {
    size_t current = editor_get_cursor(ed);
    if (offset < 0 && (size_t)(-offset) > current) {
        editor_move_cursor(ed, 0);
    } else {
        editor_move_cursor(ed, current + offset);
    }
}

void editor_insert_char(editor_t *ed, char c) {
    if (ed->gap_start == ed->gap_end) {
        editor_grow(ed, 1);
    }
    ed->buffer[ed->gap_start++] = c;
}

void editor_insert_text(editor_t *ed, const char *text) {
    size_t len = EDITOR_STRLEN(text);
    for (size_t i = 0; i < len; ++i) {
        editor_insert_char(ed, text[i]);
    }
}

void editor_backspace(editor_t *ed) {
    if (ed->gap_start > 0) {
        ed->gap_start--;
    }
}

void editor_delete(editor_t *ed) {
    if (ed->gap_end < ed->capacity) {
        ed->gap_end++;
    }
}

void editor_delete_range(editor_t *ed, size_t start, size_t end) {
    if (start >= end) return;
    size_t length = editor_get_length(ed);
    if (start >= length) return;
    if (end > length) end = length;
    
    // First, move the gap to 'end' so it covers the range's end
    editor_move_cursor(ed, end);
    
    // Then just expand the gap backwards to 'start'
    size_t count = end - start;
    ed->gap_start -= count;
}

size_t editor_get_cursor(const editor_t *ed) {
    return ed->gap_start;
}

size_t editor_get_length(const editor_t *ed) {
    return ed->gap_start + (ed->capacity - ed->gap_end);
}

char* editor_to_string(const editor_t *ed) {
    size_t len = editor_get_length(ed);
    char *str = (char *)EDITOR_MALLOC(len + 1);
    EDITOR_MEMCPY(str, ed->buffer, ed->gap_start);
    EDITOR_MEMCPY(str + ed->gap_start, ed->buffer + ed->gap_end, ed->capacity - ed->gap_end);
    str[len] = '\0';
    return str;
}

int editor_load_file(editor_t *ed, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return 0;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size > 0) {
        editor_init(ed, size + 64);
        char *temp = (char *)EDITOR_MALLOC(size);
        fread(temp, 1, size, f);
        for (long i = 0; i < size; i++) {
            editor_insert_char(ed, temp[i]);
        }
        EDITOR_FREE(temp);
    } else {
        editor_init(ed, 64);
    }
    
    fclose(f);
    return 1;
}

int editor_save_file(const editor_t *ed, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) return 0;
    
    char *s = editor_to_string(ed);
    size_t len = editor_get_length(ed);
    fwrite(s, 1, len, f);
    EDITOR_FREE(s);
    
    fclose(f);
    return 1;
}

char editor_get_char(const editor_t *ed, size_t index) {
    if (index < ed->gap_start) {
        return ed->buffer[index];
    }
    size_t gap_size = ed->gap_end - ed->gap_start;
    if (index + gap_size < ed->capacity) {
        return ed->buffer[index + gap_size];
    }
    return '\0';
}

size_t editor_find_line_start(const editor_t *ed, size_t pos) {
    if (pos == 0) return 0;
    size_t length = editor_get_length(ed);
    if (pos > length) pos = length;
    
    for (size_t i = pos; i > 0; --i) {
        if (editor_get_char(ed, i - 1) == '\n') {
            return i;
        }
    }
    return 0;
}

size_t editor_find_line_end(const editor_t *ed, size_t pos) {
    size_t length = editor_get_length(ed);
    for (size_t i = pos; i < length; ++i) {
        if (editor_get_char(ed, i) == '\n') {
            return i;
        }
    }
    return length;
}

void editor_get_row_col(const editor_t *ed, size_t pos, size_t *row, size_t *col) {
    size_t r = 0;
    size_t last_line_start = 0;
    for (size_t i = 0; i < pos; ++i) {
        if (editor_get_char(ed, i) == '\n') {
            r++;
            last_line_start = i + 1;
        }
    }
    if (row) *row = r;
    if (col) *col = pos - last_line_start;
}

void editor_move_to_line_start(editor_t *ed) {
    editor_move_cursor(ed, editor_find_line_start(ed, editor_get_cursor(ed)));
}

void editor_move_to_line_end(editor_t *ed) {
    editor_move_cursor(ed, editor_find_line_end(ed, editor_get_cursor(ed)));
}

// --- Word Motions ---

static int is_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '_');
}

static int is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int get_char_class(char c) {
    if (is_whitespace(c)) return 0;
    if (is_word_char(c)) return 1;
    return 2;
}

void editor_move_word_next(editor_t *ed) {
    size_t length = editor_get_length(ed);
    size_t pos = editor_get_cursor(ed);
    if (pos >= length) return;

    int current_class = get_char_class(editor_get_char(ed, pos));
    
    // Pula todos os caracteres da mesma classe
    while (pos < length && get_char_class(editor_get_char(ed, pos)) == current_class) {
        pos++;
    }
    
    // Se parou em um espaço, pula todos os espaços até o início da próxima palavra/pontuação
    if (pos < length && get_char_class(editor_get_char(ed, pos)) == 0) {
        while (pos < length && get_char_class(editor_get_char(ed, pos)) == 0) {
            pos++;
        }
    }
    
    editor_move_cursor(ed, pos);
}

void editor_move_word_end(editor_t *ed) {
    size_t length = editor_get_length(ed);
    size_t pos = editor_get_cursor(ed);
    if (pos >= length - 1) return;

    pos++; // Começa a procurar a partir do próximo
    
    // Se estiver em um espaço, pula até encontrar algo
    while (pos < length && get_char_class(editor_get_char(ed, pos)) == 0) {
        pos++;
    }
    
    if (pos >= length) return;
    
    int current_class = get_char_class(editor_get_char(ed, pos));
    // Pula até o fim da palavra/sequência atual
    while (pos + 1 < length && get_char_class(editor_get_char(ed, pos + 1)) == current_class) {
        pos++;
    }
    
    editor_move_cursor(ed, pos);
}

void editor_move_word_prev(editor_t *ed) {
    size_t pos = editor_get_cursor(ed);
    if (pos == 0) return;

    pos--;
    
    // Se estiver em um espaço, pula todos até encontrar uma palavra ou pontuação
    while (pos > 0 && get_char_class(editor_get_char(ed, pos)) == 0) {
        pos--;
    }
    
    int current_class = get_char_class(editor_get_char(ed, pos));
    // Pula todos os caracteres da mesma classe (para trás)
    while (pos > 0 && get_char_class(editor_get_char(ed, pos - 1)) == current_class) {
        pos--;
    }
    
    editor_move_cursor(ed, pos);
}

void editor_move_up(editor_t *ed) {
    size_t row, col;
    size_t pos = editor_get_cursor(ed);
    editor_get_row_col(ed, pos, &row, &col);
    
    if (row == 0) return;
    
    size_t line_start = editor_find_line_start(ed, pos);
    size_t prev_line_start = editor_find_line_start(ed, line_start - 1);
    size_t prev_line_end = line_start - 1;
    size_t prev_line_len = prev_line_end - prev_line_start;
    
    size_t new_col = (col > prev_line_len) ? prev_line_len : col;
    editor_move_cursor(ed, prev_line_start + new_col);
}

void editor_move_down(editor_t *ed) {
    size_t row, col;
    size_t pos = editor_get_cursor(ed);
    editor_get_row_col(ed, pos, &row, &col);
    
    size_t line_end = editor_find_line_end(ed, pos);
    size_t length = editor_get_length(ed);
    if (line_end >= length) return;
    
    size_t next_line_start = line_end + 1;
    size_t next_line_end = editor_find_line_end(ed, next_line_start);
    size_t next_line_len = next_line_end - next_line_start;
    
    size_t new_col = (col > next_line_len) ? next_line_len : col;
    editor_move_cursor(ed, next_line_start + new_col);
}

int editor_find(const editor_t *ed, const char *query, size_t start_pos) {
    size_t qlen = EDITOR_STRLEN(query);
    size_t length = editor_get_length(ed);
    if (qlen == 0 || start_pos >= length) return -1;
    
    for (size_t i = start_pos; i <= length - qlen; ++i) {
        int match = 1;
        for (size_t j = 0; j < qlen; ++j) {
            if (editor_get_char(ed, i + j) != query[j]) {
                match = 0;
                break;
            }
        }
        if (match) return (int)i;
    }
    return -1;
}

#endif // EDITOR_IMPLEMENTATION
