#define TERMINAL_IMPLEMENTATION
#include "terminal.h"

int main() {
    term_clear();
    
    term_printf_at(1, 1, "Testing new color features:");

    // Test independent fg/bg
    term_fg(TERM_COLOR_RED);
    term_printf_at(1, 3, "Only foreground RED");
    term_bg(TERM_BG_BLUE);
    term_printf_at(1, 4, "Foreground RED and background BLUE (set separately)");
    term_reset();

    // Test bright colors
    term_fg(TERM_COLOR_BRIGHT_YELLOW);
    term_printf_at(1, 6, "Bright Yellow Foreground");
    term_bg(TERM_BG_BRIGHT_MAGENTA);
    term_printf_at(1, 7, "Bright Yellow Foreground and Bright Magenta Background");
    term_reset();

    // Test 256 colors
    term_fg_256(208); // Orange
    term_printf_at(1, 9, "256-color: Orange Foreground (208)");
    term_bg_256(57);  // Purple
    term_printf_at(1, 10, "256-color: Orange on Purple (208 on 57)");
    term_reset();

    // Test RGB colors
    term_fg_rgb(0, 255, 128); // Spring Green
    term_printf_at(1, 12, "RGB: Spring Green Foreground (0, 255, 128)");
    term_bg_rgb(128, 0, 0);   // Dark Red
    term_printf_at(1, 13, "RGB: Spring Green on Dark Red");
    term_reset();

    term_printf_at(1, 15, "Press ENTER to exit...");
    getchar();
    term_clear();

    return 0;
}
