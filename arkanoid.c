// xv6 port of the mode-13h Arkanoid clone.
// Screen: open "display", ioctl mode 0x13, write pixels, ioctl palette (see imshow).
// Keys: console direct mode, same ioctl as directcat. Each a/d (or arrow)
// press moves the paddle one step. Space launches, r restarts, q or Esc quits.
#include "types.h"
#include "user.h"
#include "fcntl.h"

enum {
    W = 320,
    H = 200,
    HUD = 16,
    COLS = 10,
    ROWS = 6,
    BW = 28,
    BH = 8,
    GAPX = 3,
    GAPY = 3,
    OX = 11,
    OY = 22,
    BALL = 4,
    PADDLE_H = 5,
    PADDLE_Y = 186,
    PADDLE_W = 42,
    FP = 8,
    NLEVELS = 3,
    NLIVES = 3,
};

enum { ST_SERVE, ST_PLAY, ST_OVER, ST_WIN };

// 5x7 font, bits 4..0 = left..right. Index 0 is space; 1-10 = 0-9; 11-36 = A-Z.
static const uchar FONT[][7] = {
    {0,0,0,0,0,0,0},
    {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e},
    {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e},
    {0x0e,0x11,0x01,0x06,0x08,0x10,0x1f},
    {0x0e,0x11,0x01,0x06,0x01,0x11,0x0e},
    {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02},
    {0x1f,0x10,0x1e,0x01,0x01,0x11,0x0e},
    {0x06,0x08,0x10,0x1e,0x11,0x11,0x0e},
    {0x1f,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e},
    {0x0e,0x11,0x11,0x0f,0x01,0x02,0x0c},
    {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11},
    {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e},
    {0x0e,0x11,0x10,0x10,0x10,0x11,0x0e},
    {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e},
    {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f},
    {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10},
    {0x0e,0x11,0x10,0x17,0x11,0x11,0x0e},
    {0x11,0x11,0x11,0x1f,0x11,0x11,0x11},
    {0x0e,0x04,0x04,0x04,0x04,0x04,0x0e},
    {0x01,0x01,0x01,0x01,0x11,0x11,0x0e},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1f},
    {0x11,0x1b,0x15,0x15,0x11,0x11,0x11},
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e},
    {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10},
    {0x0e,0x11,0x11,0x11,0x15,0x12,0x0d},
    {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11},
    {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e},
    {0x1f,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0e},
    {0x11,0x11,0x11,0x11,0x11,0x0a,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x1b,0x11},
    {0x11,0x11,0x0a,0x04,0x0a,0x11,0x11},
    {0x11,0x11,0x0a,0x04,0x04,0x04,0x04},
    {0x1f,0x01,0x02,0x04,0x08,0x10,0x1f},
};

static uchar BUF[W * H];
static int dispfd = -1;
static int quit_game;

static uchar bricks[ROWS][COLS];
static int bricks_left;
static int paddle_x, paddle_w;
static int ball_x, ball_y, ball_vx, ball_vy;
static int score, lives, level, state;
static int steps;      /* +right / -left, one per key press */
static int key_space;
static int space_hit;

static int tofp(int x) { return x << FP; }
static int fromfp(int x) { return x >> FP; }

static int abs_i(int x) { return x < 0 ? -x : x; }

static int clamp(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// ioctl value is (palette#, R, G, B), each channel 0..63. See imshow.c.
static void pal(uchar i, uchar r, uchar g, uchar b)
{
    ioctl(dispfd, 2, (int)(((unsigned)i << 24) | ((unsigned)r << 16) |
                           ((unsigned)g << 8) | b));
}

static void init_palette(void)
{
    pal(0,  0,  0,  0);
    pal(1,  4,  4, 12);     // navy playfield
    pal(2, 63, 63, 63);     // white
    pal(3, 63, 55, 12);     // gold ball / highlight
    pal(4, 20, 50, 55);     // paddle
    pal(5, 12, 12, 18);     // hud bar
    pal(6,  8,  8, 14);     // shadow
    pal(7, 50, 50, 55);     // silver
    pal(8, 55, 16, 12);     // red
    pal(9, 55, 32,  8);     // orange
    pal(10, 55, 48, 8);     // yellow
    pal(11, 16, 48, 16);    // green
    pal(12, 12, 42, 50);    // cyan
    pal(13, 16, 20, 55);    // blue
    pal(14, 48, 16, 48);    // magenta
    pal(15, 40, 40, 48);    // dim text
}

// Standard text-mode colors. Mode 3 does not put the old DAC back.
static void restore_text_palette(void)
{
    static const uchar textpal[16][3] = {
        {0, 0, 0}, {0, 0, 42}, {0, 42, 0}, {0, 42, 42},
        {42, 0, 0}, {42, 0, 42}, {42, 21, 0}, {42, 42, 42},
        {21, 21, 21}, {21, 21, 63}, {21, 63, 21}, {21, 63, 63},
        {63, 21, 21}, {63, 21, 63}, {63, 63, 21}, {63, 63, 63},
    };
    int i;
    for (i = 0; i < 16; i++)
        pal(i, textpal[i][0], textpal[i][1], textpal[i][2]);
}

static void put(int x, int y, uchar c)
{
    if ((unsigned)x < W && (unsigned)y < H)
        BUF[y * W + x] = c;
}

static void fill(int x, int y, int w, int h, uchar c)
{
    int i, j;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    for (i = 0; i < h; i++)
        for (j = 0; j < w; j++)
            BUF[(y + i) * W + (x + j)] = c;
}

// The display offset only moves forward, and there is no seek, so each
// frame is a new fd whose write starts at pixel 0.
static void blit(void)
{
    int fd;
    fd = open("display", O_WRONLY);
    if (fd < 0) {
        quit_game = 1;
        return;
    }
    if (write(fd, BUF, W * H) != W * H)
        quit_game = 1;
    close(fd);
}

static void wait_frame(void)
{
    if (sleep(2) < 0)
        quit_game = 1;
}

static int font_index(int ch)
{
    if (ch == ' ') return 0;
    if (ch >= '0' && ch <= '9') return 1 + (ch - '0');
    if (ch >= 'A' && ch <= 'Z') return 11 + (ch - 'A');
    if (ch >= 'a' && ch <= 'z') return 11 + (ch - 'a');
    return 0;
}

static void draw_char(int x, int y, int ch, uchar c)
{
    const uchar *g = FONT[font_index(ch)];
    int i, j;
    for (i = 0; i < 7; i++)
        for (j = 0; j < 5; j++)
            if (g[i] & (0x10 >> j))
                put(x + j, y + i, c);
}

static void draw_str(int x, int y, const char *s, uchar c)
{
    while (*s) {
        draw_char(x, y, *s, c);
        x += 6;
        s++;
    }
}

static void draw_num(int x, int y, int n, int digits, uchar c)
{
    char buf[8];
    int i;
    if (digits > 7) digits = 7;
    for (i = digits - 1; i >= 0; i--) {
        buf[i] = '0' + (n % 10);
        n /= 10;
    }
    buf[digits] = 0;
    draw_str(x, y, buf, c);
}

static int overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static uchar row_color(int r)
{
    static const uchar cols[ROWS] = { 8, 9, 10, 11, 12, 13 };
    return cols[r];
}

static void setup_level(void)
{
    int r, c;
    bricks_left = 0;
    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
            uchar v = 0;
            if (level == 0)
                v = 1;
            else if (level == 1)
                v = ((r + c) & 1) ? 1 : 0;
            else {
                if (c >= r && c < COLS - r)
                    v = (r < 2) ? 2 : 1;
            }
            bricks[r][c] = v;
            if (v)
                bricks_left++;
        }
    }
}

static void serve_ball(void)
{
    paddle_w = PADDLE_W - level * 4;
    if (paddle_w < 28)
        paddle_w = 28;
    ball_x = tofp(paddle_x + paddle_w / 2 - BALL / 2);
    ball_y = tofp(PADDLE_Y - BALL - 1);
    ball_vx = 0;
    ball_vy = 0;
    state = ST_SERVE;
}

static void launch_ball(void)
{
    int dir = (paddle_x + paddle_w / 2 > W / 2) ? -1 : 1;
    int spd = 0x160 + level * 0x30;
    ball_vx = dir * spd;
    ball_vy = -(0x1c0 + level * 0x20);
    state = ST_PLAY;
}

static void reset_game(void)
{
    score = 0;
    lives = NLIVES;
    level = 0;
    paddle_x = (W - PADDLE_W) / 2;
    paddle_w = PADDLE_W;
    setup_level();
    serve_ball();
}

static void smash(int r, int c)
{
    if (bricks[r][c] > 1) {
        bricks[r][c]--;
        score += 5;
        return;
    }
    bricks[r][c] = 0;
    bricks_left--;
    score += 10;
}

static int hit_bricks(int px, int py)
{
    int r, c;
    int bx1 = px, by1 = py;
    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
            int x, y;
            if (!bricks[r][c])
                continue;
            x = OX + c * (BW + GAPX);
            y = OY + r * (BH + GAPY);
            if (overlap(bx1, by1, BALL, BALL, x, y, BW, BH)) {
                smash(r, c);
                return 1;
            }
        }
    }
    return 0;
}

static void poll_kbd(void)
{
    uchar keys[32];
    int n, i;

    n = read(0, keys, sizeof(keys));
    if (n <= 0)
        return;
    for (i = 0; i < n; i++) {
        switch (keys[i]) {
        case 'a': case 'A': case 0xe4:   /* a, left */
            steps--;
            break;
        case 'd': case 'D': case 0xe5:   /* d, right */
            steps++;
            break;
        case ' ':
            key_space = 1; space_hit = 1;
            break;
        case 'r': case 'R':
            if (state == ST_OVER || state == ST_WIN)
                reset_game();
            break;
        case 'q': case 'Q': case 033:    /* q, Esc */
            quit_game = 1;
            break;
        }
    }
}

static void move_paddle(void)
{
    paddle_x += 10 * steps;
    steps = 0;
    paddle_x = clamp(paddle_x, 2, W - 2 - paddle_w);
}

static void move_ball(void)
{
    int nx, ny, px, py;
    int maxv = 0x280 + level * 0x40;

    nx = ball_x + ball_vx;
    px = fromfp(nx);
    py = fromfp(ball_y);
    if (px < 2) {
        nx = tofp(2);
        ball_vx = abs_i(ball_vx);
    } else if (px + BALL > W - 2) {
        nx = tofp(W - 2 - BALL);
        ball_vx = -abs_i(ball_vx);
    } else if (hit_bricks(px, py)) {
        ball_vx = -ball_vx;
        nx = ball_x;
    }
    ball_x = nx;

    ny = ball_y + ball_vy;
    px = fromfp(ball_x);
    py = fromfp(ny);
    if (py < HUD + 1) {
        ny = tofp(HUD + 1);
        ball_vy = abs_i(ball_vy);
    } else if (hit_bricks(px, py)) {
        ball_vy = -ball_vy;
        ny = ball_y;
    } else if (ball_vy > 0 &&
               overlap(px, py, BALL, BALL, paddle_x, PADDLE_Y, paddle_w, PADDLE_H)) {
        int hit = (px + BALL / 2) - (paddle_x + paddle_w / 2);
        ball_vx += hit * 12;
        ball_vx = clamp(ball_vx, -maxv, maxv);
        if (abs_i(ball_vx) < 0x80)
            ball_vx = ball_vx < 0 ? -0x80 : 0x80;
        ball_vy = -abs_i(ball_vy);
        if (abs_i(ball_vy) < 0x140)
            ball_vy = -0x140;
        ny = tofp(PADDLE_Y - BALL - 1);
    }
    ball_y = ny;

    if (fromfp(ball_y) > H) {
        lives--;
        if (lives <= 0)
            state = ST_OVER;
        else
            serve_ball();
        return;
    }

    if (bricks_left <= 0) {
        level++;
        if (level >= NLEVELS) {
            state = ST_WIN;
            return;
        }
        setup_level();
        serve_ball();
    }
}

static void draw_beveled(int x, int y, int w, int h, uchar c)
{
    fill(x, y, w, h, c);
    fill(x, y, w, 1, 3);
    fill(x, y, 1, h, 3);
    fill(x, y + h - 1, w, 1, 6);
    fill(x + w - 1, y, 1, h, 6);
}

static void draw_scene(void)
{
    int r, c, i;
    int bx, by;

    fill(0, 0, W, H, 1);
    fill(0, 0, W, HUD, 5);
    fill(0, HUD, W, 1, 6);
    fill(0, 0, 2, H, 7);
    fill(W - 2, 0, 2, H, 7);

    draw_str(4, 5, "SCORE", 15);
    draw_num(36, 5, score, 4, 2);
    draw_str(80, 5, "LIVES", 15);
    for (i = 0; i < lives; i++)
        fill(114 + i * 8, 6, 6, 4, 3);
    draw_str(150, 5, "LV", 15);
    draw_num(164, 5, level + 1, 1, 2);
    draw_str(188, 5, "ARKANOID", 3);
    draw_str(262, 5, "Q QUIT", 15);

    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
            int x, y;
            uchar col;
            if (!bricks[r][c])
                continue;
            x = OX + c * (BW + GAPX);
            y = OY + r * (BH + GAPY);
            col = (bricks[r][c] > 1) ? 7 : row_color(r);
            draw_beveled(x, y, BW, BH, col);
        }
    }

    draw_beveled(paddle_x, PADDLE_Y, paddle_w, PADDLE_H, 4);

    bx = fromfp(ball_x);
    by = fromfp(ball_y);
    fill(bx, by, BALL, BALL, 3);
    put(bx + 1, by + 1, 2);
    put(bx + 2, by + 1, 2);

    if (state == ST_SERVE)
        draw_str(116, 110, "PRESS SPACE", 2);
    else if (state == ST_OVER) {
        draw_str(128, 100, "GAME OVER", 8);
        draw_str(116, 114, "PRESS SPACE", 2);
    } else if (state == ST_WIN) {
        draw_str(134, 100, "YOU WIN", 10);
        draw_str(116, 114, "PRESS SPACE", 2);
    }
}

static void drain_kbd(void)
{
    uchar scratch[32];
    int i;
    for (i = 0; i < 8; i++)
        if (read(0, scratch, sizeof(scratch)) <= 0)
            break;
}

static void arkanoid(void)
{
    init_palette();
    steps = key_space = 0;
    drain_kbd();
    space_hit = 0;
    reset_game();

    while (!quit_game) {
        poll_kbd();
        if (quit_game)
            break;
        move_paddle();

        if (state == ST_SERVE) {
            ball_x = tofp(paddle_x + paddle_w / 2 - BALL / 2);
            ball_y = tofp(PADDLE_Y - BALL - 1);
            if (space_hit)
                launch_ball();
        } else if (state == ST_PLAY) {
            move_ball();
        } else if ((state == ST_OVER || state == ST_WIN) && space_hit) {
            reset_game();
        }
        space_hit = 0;

        draw_scene();
        wait_frame();
        blit();
    }
}

int
main(void)
{
    dispfd = open("display", O_WRONLY);
    if (dispfd < 0) {
        printf(2, "arkanoid: cannot open display\n");
        exit();
    }
    if (ioctl(dispfd, 1, 0x13) < 0) {
        printf(2, "arkanoid: mode 13 failed\n");
        close(dispfd);
        exit();
    }
    ioctl(0, 2, 1); // DIRECT mode, same as directcat

    arkanoid();

    // Line mode first so its console message is wiped by the text restore.
    ioctl(0, 2, 0);
    restore_text_palette();
    ioctl(dispfd, 1, 0x03);
    close(dispfd);
    exit();
}
