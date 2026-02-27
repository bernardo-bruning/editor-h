#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <time.h>

#define TERMINAL_IMPLEMENTATION
#include "terminal.h"

// Configurações do Jogo
#define BALL_CHAR "O"
#define PADDLE_CHAR "█"
#define PADDLE_HEIGHT 4
#define SLEEP_TIME 50000 // 50ms

struct {
    int width, height;
    float ball_x, ball_y;
    float ball_dx, ball_dy;
    int paddle1_y, paddle2_y;
    int score1, score2;
    int running;
} game;

struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    term_cursor_show(1);
    term_reset();
    term_clear();
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void init_game() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    game.width = w.ws_col;
    game.height = w.ws_row - 2; // Reserva espaço para placar

    game.ball_x = game.width / 2;
    game.ball_y = game.height / 2;
    game.ball_dx = 1.0f;
    game.ball_dy = 0.5f;

    game.paddle1_y = game.height / 2 - PADDLE_HEIGHT / 2;
    game.paddle2_y = game.height / 2 - PADDLE_HEIGHT / 2;
    
    game.score1 = 0;
    game.score2 = 0;
    game.running = 1;
}

void reset_ball() {
    game.ball_x = game.width / 2;
    game.ball_y = game.height / 2;
    game.ball_dx = (game.ball_dx > 0) ? -1.0f : 1.0f;
    game.ball_dy = 0.5f;
}

void update() {
    game.ball_x += game.ball_dx;
    game.ball_y += game.ball_dy;

    // Colisão teto/chão
    if (game.ball_y <= 1 || game.ball_y >= game.height) {
        game.ball_dy = -game.ball_dy;
    }

    // Colisão Raquete 1
    if (game.ball_x <= 2) {
        if (game.ball_y >= game.paddle1_y && game.ball_y <= game.paddle1_y + PADDLE_HEIGHT) {
            game.ball_dx = -game.ball_dx;
            game.ball_x = 3;
        } else if (game.ball_x <= 0) {
            game.score2++;
            reset_ball();
        }
    }

    // Colisão Raquete 2
    if (game.ball_x >= game.width - 1) {
        if (game.ball_y >= game.paddle2_y && game.ball_y <= game.paddle2_y + PADDLE_HEIGHT) {
            game.ball_dx = -game.ball_dx;
            game.ball_x = game.width - 2;
        } else if (game.ball_x >= game.width) {
            game.score1++;
            reset_ball();
        }
    }

    // IA Simples para Raquete 2
    if (game.ball_y > game.paddle2_y + PADDLE_HEIGHT / 2 && game.paddle2_y < game.height - PADDLE_HEIGHT) {
        game.paddle2_y++;
    } else if (game.ball_y < game.paddle2_y + PADDLE_HEIGHT / 2 && game.paddle2_y > 1) {
        game.paddle2_y--;
    }
}

void draw() {
    term_clear();
    
    // Desenha Placar
    term_set_color(TERM_COLOR_YELLOW, TERM_BG_BLACK);
    term_printf_at(game.width / 2 - 10, game.height + 1, "Player 1: %d  |  CPU: %d", game.score1, game.score2);
    
    // Desenha Paredes
    term_set_color(TERM_COLOR_WHITE, TERM_BG_BLACK);
    for (int i = 1; i <= game.width; i++) {
        term_printf_at(i, 1, "-");
        term_printf_at(i, game.height, "-");
    }

    // Desenha Raquete 1
    term_set_color(TERM_COLOR_CYAN, TERM_BG_BLACK);
    for (int i = 0; i < PADDLE_HEIGHT; i++) {
        term_printf_at(2, game.paddle1_y + i, PADDLE_CHAR);
    }

    // Desenha Raquete 2
    term_set_color(TERM_COLOR_MAGENTA, TERM_BG_BLACK);
    for (int i = 0; i < PADDLE_HEIGHT; i++) {
        term_printf_at(game.width - 1, game.paddle2_y + i, PADDLE_CHAR);
    }

    // Desenha Bola
    term_set_color(TERM_COLOR_BRIGHT_WHITE, TERM_BG_BLACK);
    term_printf_at((int)game.ball_x, (int)game.ball_y, BALL_CHAR);

    term_gotoxy(1, 1);
    fflush(stdout);
}

void process_input() {
    struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
    if (poll(&pfd, 1, 0) > 0) {
        char c;
        read(STDIN_FILENO, &c, 1);
        if (c == 'q') game.running = 0;
        if (c == 'w' && game.paddle1_y > 2) game.paddle1_y--;
        if (c == 's' && game.paddle1_y < game.height - PADDLE_HEIGHT) game.paddle1_y++;
        
        // Suporte a setas
        if (c == 27) {
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
                if (seq[0] == '[') {
                    if (seq[1] == 'A' && game.paddle1_y > 2) game.paddle1_y--;
                    if (seq[1] == 'B' && game.paddle1_y < game.height - PADDLE_HEIGHT) game.paddle1_y++;
                }
            }
        }
    }
}

int main() {
    enable_raw_mode();
    term_cursor_show(0);
    init_game();

    while (game.running) {
        process_input();
        update();
        draw();
        usleep(SLEEP_TIME);
    }

    return 0;
}
