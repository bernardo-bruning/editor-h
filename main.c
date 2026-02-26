#define EDITOR_IMPLEMENTATION
#include "editor.h"
#include "tap.h"
#include <string.h>
#include <stdlib.h>

void test_basic() {
    editor_t ed;
    editor_init(&ed, 10);
    
    editor_insert_text(&ed, "Hello World");
    char *s1 = editor_to_string(&ed);
    ok(strcmp(s1, "Hello World") == 0, "Insert text 'Hello World'");
    free(s1);

    editor_backspace(&ed);
    char *s2 = editor_to_string(&ed);
    ok(strcmp(s2, "Hello Worl") == 0, "Backspace removes last char");
    free(s2);

    editor_move_cursor(&ed, 6);
    editor_insert_text(&ed, "Beautiful ");
    char *s3 = editor_to_string(&ed);
    ok(strcmp(s3, "Hello Beautiful Worl") == 0, "Move and insert text");
    free(s3);

    editor_move_cursor(&ed, 0);
    editor_delete(&ed);
    char *s4 = editor_to_string(&ed);
    ok(strcmp(s4, "ello Beautiful Worl") == 0, "Delete first char");
    free(s4);

    editor_free(&ed);
}

void test_navigation() {
    editor_t ed;
    editor_init(&ed, 10);
    editor_insert_text(&ed, "Line 1\nLine 2\nLine 3");
    
    ok(editor_get_cursor(&ed) == 20, "Cursor at end after insertion");
    
    editor_move_up(&ed);
    size_t row, col;
    editor_get_row_col(&ed, editor_get_cursor(&ed), &row, &col);
    ok(row == 1 && col == 6, "Move up to Line 2");
    
    editor_move_up(&ed);
    editor_get_row_col(&ed, editor_get_cursor(&ed), &row, &col);
    ok(row == 0 && col == 6, "Move up to Line 1");
    
    editor_move_to_line_start(&ed);
    editor_get_row_col(&ed, editor_get_cursor(&ed), &row, &col);
    ok(col == 0, "Move to line start");
    
    editor_move_down(&ed);
    editor_get_row_col(&ed, editor_get_cursor(&ed), &row, &col);
    ok(row == 1 && col == 0, "Move down to Line 2");
    
    editor_move_to_line_end(&ed);
    editor_get_row_col(&ed, editor_get_cursor(&ed), &row, &col);
    ok(col == 6, "Move to line end");

    editor_free(&ed);
}

void test_search() {
    editor_t ed;
    editor_init(&ed, 10);
    editor_insert_text(&ed, "The quick brown fox jumps over the lazy dog");
    
    ok(editor_find(&ed, "fox", 0) == 16, "Find 'fox'");
    ok(editor_find(&ed, "lazy", 0) == 35, "Find 'lazy'");
    ok(editor_find(&ed, "cat", 0) == -1, "Should not find 'cat'");
    
    editor_free(&ed);
}

int main() {
    plan(13);
    test_basic();
    test_navigation();
    test_search();
    return done_testing();
}
