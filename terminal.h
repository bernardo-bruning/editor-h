/*
 * terminal.h - STB-style terminal UI library
 * 
 * To use this library, do:
 * #define TERMINAL_IMPLEMENTATION
 * #include "terminal.h"
 */

#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Cores de Frente (Foreground)
#define TERM_COLOR_BLACK   30
#define TERM_COLOR_RED     31
#define TERM_COLOR_GREEN   32
#define TERM_COLOR_YELLOW  33
#define TERM_COLOR_BLUE    34
#define TERM_COLOR_MAGENTA 35
#define TERM_COLOR_CYAN    36
#define TERM_COLOR_WHITE   37

// Cores de Frente Brilhantes (Bright Foreground)
#define TERM_COLOR_BRIGHT_BLACK   90
#define TERM_COLOR_BRIGHT_RED     91
#define TERM_COLOR_BRIGHT_GREEN   92
#define TERM_COLOR_BRIGHT_YELLOW  93
#define TERM_COLOR_BRIGHT_BLUE    94
#define TERM_COLOR_BRIGHT_MAGENTA 95
#define TERM_COLOR_BRIGHT_CYAN    96
#define TERM_COLOR_BRIGHT_WHITE   97

// Cores de Fundo (Background)
#define TERM_BG_BLACK      40
#define TERM_BG_RED        41
#define TERM_BG_GREEN      42
#define TERM_BG_YELLOW     43
#define TERM_BG_BLUE       44
#define TERM_BG_MAGENTA    45
#define TERM_BG_CYAN       46
#define TERM_BG_WHITE      47

// Cores de Fundo Brilhantes (Bright Background)
#define TERM_BG_BRIGHT_BLACK      100
#define TERM_BG_BRIGHT_RED        101
#define TERM_BG_BRIGHT_GREEN      102
#define TERM_BG_BRIGHT_YELLOW     103
#define TERM_BG_BRIGHT_BLUE       104
#define TERM_BG_BRIGHT_MAGENTA    105
#define TERM_BG_BRIGHT_CYAN       106
#define TERM_BG_BRIGHT_WHITE      107

// Estilos
#define TERM_STYLE_RESET     0
#define TERM_STYLE_BOLD      1
#define TERM_STYLE_DIM       2
#define TERM_STYLE_ITALIC    3
#define TERM_STYLE_UNDERLINE 4
#define TERM_STYLE_BLINK     5

// Limpa a tela
void term_clear(void);

// Move o cursor para a posição x, y (1-indexed)
void term_gotoxy(int x, int y);

// Define as cores de frente e fundo
void term_set_color(int fg, int bg);

// Define apenas a cor de frente
void term_fg(int color);

// Define apenas a cor de fundo
void term_bg(int color);

// Define cor de frente (256 cores, 0-255)
void term_fg_256(int color);

// Define cor de fundo (256 cores, 0-255)
void term_bg_256(int color);

// Define cor de frente (RGB, 0-255 cada)
void term_fg_rgb(int r, int g, int b);

// Define cor de fundo (RGB, 0-255 cada)
void term_bg_rgb(int r, int g, int b);

// Define o estilo (Bold, Underline, etc)
void term_set_style(int style);

// Reseta todas as cores e estilos
void term_reset(void);

// Escreve um texto formatado em uma posição específica
void term_printf_at(int x, int y, const char *fmt, ...);

// Oculta/Mostra o cursor
void term_cursor_show(int show);

#ifdef __cplusplus
}
#endif

#endif // TERMINAL_H

#ifdef TERMINAL_IMPLEMENTATION
#include <stdarg.h>

void term_clear(void) {
    printf("\033[2J\033[H");
}

void term_gotoxy(int x, int y) {
    printf("\033[%d;%dH", y, x);
}

void term_set_color(int fg, int bg) {
    printf("\033[%d;%dm", fg, bg);
}

void term_fg(int color) {
    printf("\033[%dm", color);
}

void term_bg(int color) {
    printf("\033[%dm", color);
}

void term_fg_256(int color) {
    printf("\033[38;5;%dm", color);
}

void term_bg_256(int color) {
    printf("\033[48;5;%dm", color);
}

void term_fg_rgb(int r, int g, int b) {
    printf("\033[38;2;%d;%d;%dm", r, g, b);
}

void term_bg_rgb(int r, int g, int b) {
    printf("\033[48;2;%d;%d;%dm", r, g, b);
}

void term_set_style(int style) {
    printf("\033[%dm", style);
}

void term_reset(void) {
    printf("\033[0m");
}

void term_printf_at(int x, int y, const char *fmt, ...) {
    term_gotoxy(x, y);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void term_cursor_show(int show) {
    if (show) printf("\033[?25h");
    else printf("\033[?25l");
}

#endif // TERMINAL_IMPLEMENTATION
