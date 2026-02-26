#ifndef TAP_H
#define TAP_H

#include <stdio.h>
#include <stdarg.h>

static int _tap_test_count = 0;
static int _tap_test_failed = 0;
static int _tap_plan_count = 0;

static void plan(int count) {
    _tap_plan_count = count;
    printf("1..%d\n", count);
}

static void ok(int condition, const char *fmt, ...) {
    _tap_test_count++;
    va_list args;
    va_start(args, fmt);
    
    if (condition) {
        printf("ok %d - ", _tap_test_count);
    } else {
        printf("not ok %d - ", _tap_test_count);
        _tap_test_failed++;
    }
    
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

static void diag(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("# ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

static int done_testing() {
    if (_tap_plan_count == 0) {
        printf("1..%d\n", _tap_test_count);
    }
    
    if (_tap_test_failed > 0) {
        diag("Failed %d tests out of %d", _tap_test_failed, _tap_test_count);
        return 1;
    }
    diag("All %d tests passed", _tap_test_count);
    return 0;
}

#endif /* TAP_H */
