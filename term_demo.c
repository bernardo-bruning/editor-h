#define TERMINAL_IMPLEMENTATION
#include "terminal.h"

int main() {
    term_clear();
    term_cursor_show(0);

    // Título
    term_set_style(TERM_STYLE_BOLD);
    term_set_color(TERM_COLOR_CYAN, TERM_BG_BLACK);
    term_printf_at(10, 2, "--- FORMULÁRIO DE CADASTRO ---");
    term_reset();

    // Rótulos
    term_set_style(TERM_STYLE_DIM);
    term_printf_at(5, 5, "Nome:");
    term_printf_at(5, 7, "Email:");
    term_printf_at(5, 9, "Idade:");
    term_reset();

    // Campos (marcação visual)
    term_set_color(TERM_COLOR_BLACK, TERM_BG_WHITE);
    term_printf_at(12, 5, "                    "); // Campo Nome
    term_printf_at(12, 7, "                    "); // Campo Email
    term_printf_at(12, 9, "    ");                 // Campo Idade
    term_reset();

    // Simulando preenchimento
    term_set_color(TERM_COLOR_BLUE, TERM_BG_WHITE);
    term_printf_at(12, 5, "Bernardo Bruning");
    term_printf_at(12, 7, "contato@exemplo.com");
    term_printf_at(12, 9, "25");
    term_reset();

    term_printf_at(1, 12, "Pressione ENTER para sair...");
    getchar();

    term_cursor_show(1);
    term_clear();
    return 0;
}
