#include <graph.h>
#include <i86.h>
#include <dos.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

extern int getch(void);
extern int kbhit(void);

#include <conio.h>

#define SCREEN_WIDTH  40
#define SCREEN_ROWS   25

#define ATTR_NORMAL   0x17
#define ATTR_REVERSE  0x70

static uint8_t saved_attr  = 0x07;
static uint8_t current_attr = ATTR_NORMAL;
static int     cur_x = 0, cur_y = 0;

static uint8_t far *get_vbuf(void)
{
    return (uint8_t far *)MK_FP(0xB800, 0);
}

static void fill_screen(uint8_t attr)
{
    static union REGS r;
    r.h.ah = 0x06;
    r.h.al = 0x00;
    r.h.bh = attr;
    r.h.ch = 0;
    r.h.cl = 0;
    r.h.dh = SCREEN_ROWS - 1;
    r.h.dl = SCREEN_WIDTH - 1;
    int86(0x10, &r, &r);
}

void gotoxy(int x, int y)
{
    static union REGS r;
    cur_x = x;
    cur_y = y;
    r.h.ah = 0x02;
    r.h.bh = 0x00;
    r.h.dh = (uint8_t)y;
    r.h.dl = (uint8_t)x;
    int86(0x10, &r, &r);
}

void clrscr(void)
{
    current_attr = ATTR_NORMAL;
    fill_screen(ATTR_NORMAL);
    gotoxy(0, 0);
}

static void vbuf_putc(char c)
{
    uint8_t far *vbuf = get_vbuf();
    int pos;
    if (c == '\r') {
        cur_x = 0;
        return;
    }
    if (c == '\n') {
        cur_x = 0;
        if (cur_y < SCREEN_ROWS - 1) cur_y++;
        return;
    }
    if (cur_y >= SCREEN_ROWS) return;
    /* Skip the very last cell (row 24, col 39) — writing it triggers a scroll */
    if (cur_y == SCREEN_ROWS - 1 && cur_x == SCREEN_WIDTH - 1) return;
    pos = cur_y * SCREEN_WIDTH + cur_x;
    vbuf[pos * 2]     = (uint8_t)c;
    vbuf[pos * 2 + 1] = current_attr;
    if (++cur_x >= SCREEN_WIDTH) {
        cur_x = 0;
        if (cur_y < SCREEN_ROWS - 1) cur_y++;
    }
}

void cputc(char c)
{
    vbuf_putc(c);
}

int cputs(const char *s)
{
    while (*s)
        vbuf_putc(*s++);
    return 0;
}

void cclear(int n)
{
    while (n-- > 0)
        vbuf_putc(' ');
}

void screensize(uint8_t *w, uint8_t *h)
{
    *w = SCREEN_WIDTH;
    *h = SCREEN_ROWS - 1;
}

char cgetc(void)
{
    int c;
    if (!kbhit())
        return 0;
    c = getch();
    if (c == 0) {
        c = getch();
        switch (c) {
            case 0x48: return KEY_UP_ARROW;
            case 0x50: return KEY_DOWN_ARROW;
            case 0x4B: return KEY_LEFT_ARROW;
            case 0x4D: return KEY_RIGHT_ARROW;
            default:   return 0;
        }
    }
    return (char)c;
}

void revers(uint8_t on)
{
    current_attr = on ? ATTR_REVERSE : ATTR_NORMAL;
}

void set_video_mode(VideoMode mode)
{
    _setvideomode(mode == VIDEO_MODE_40COL ? _TEXTC40 : _TEXTC80);
}

void set_screen_bg_blue(void)
{
    uint8_t far *vbuf = get_vbuf();
    saved_attr = vbuf[1];
    fill_screen(ATTR_NORMAL);
    gotoxy(0, 0);
}

void restore_screen_bg(void)
{
    fill_screen(saved_attr);
    gotoxy(0, 0);
}

#define BOX_TL '\xDA'
#define BOX_TR '\xBF'
#define BOX_BL '\xC0'
#define BOX_BR '\xD9'
#define BOX_H  '\xC4'
#define BOX_V  '\xB3'

#define putc_at(px,py,attr,ch) \
    do { static union REGS _r; \
    _r.h.ah=0x02; _r.h.bh=0; _r.h.dh=(uint8_t)(py); _r.h.dl=(uint8_t)(px); int86(0x10,&_r,&_r); \
    _r.h.ah=0x09; _r.h.al=(uint8_t)(ch); _r.h.bh=0; _r.h.bl=(attr); _r.x.cx=1; int86(0x10,&_r,&_r); \
    } while(0)


void draw_box(int x, int y, int w, int h)
{
    int i;
    uint8_t a = ATTR_NORMAL;

    putc_at(x,       y,       a, BOX_TL);
    for (i = 1; i < w-1; i++) { putc_at(x+i, y, a, BOX_H); }
    putc_at(x+w-1,   y,       a, BOX_TR);

    for (i = 1; i < h-1; i++) {
        putc_at(x,     y+i, a, BOX_V);
        putc_at(x+w-1, y+i, a, BOX_V);
    }

    putc_at(x,       y+h-1, a, BOX_BL);
    for (i = 1; i < w-1; i++) { putc_at(x+i, y+h-1, a, BOX_H); }
    putc_at(x+w-1,   y+h-1, a, BOX_BR);

    gotoxy(0, 0);
}

void cursor(bool on)
{
    static union REGS r;
    r.h.ah = 0x01;
    r.h.bh = 0x00;
    if (on) {
        r.h.ch = 0x06;
        r.h.cl = 0x07;
    } else {
        r.h.ch = 0x20;
        r.h.cl = 0x00;
    }
    int86(0x10, &r, &r);
}
