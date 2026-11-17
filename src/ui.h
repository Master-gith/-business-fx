#ifndef UI_H
#define UI_H

#include <stddef.h>
#include <gint/display.h>
#include <gint/keyboard.h>
#include "data.h"

#define SCR_W 396
#define SCR_H 224
#define HEAD_H 18
#define FOOT_Y 208

#define COL_BG     C_WHITE
#define COL_TXT    C_BLACK
#define COL_ACC    C_RGB(3, 10, 20)
#define COL_SEL    C_RGB(25, 28, 31)
#define COL_GRID   C_RGB(25, 25, 25)
#define COL_MUTE   C_RGB(12, 12, 12)
#define COL_TR     C_RGB(2, 8, 26)
#define COL_TC     C_RGB(27, 8, 2)
#define COL_FC     C_RGB(14, 14, 14)
#define COL_PROFIT C_RGB(24, 30, 24)
#define COL_LOSS   C_RGB(31, 24, 24)
#define COL_GOOD   C_RGB(4, 20, 6)
#define COL_BAD    C_RGB(26, 4, 4)

extern int g_lh; /* line height  */
extern int g_th; /* text height  */

void ui_init(void);
int  ui_w(const char *s);
void ui_text(int x, int y, int color, const char *s);
void ui_text_r(int xr, int y, int color, const char *s);
void ui_header(const char *title);
void ui_footer(const char *const f[6]);
void ui_msg(const char *s);
int  ui_wrap(const char *s, int maxw, const char **lines, int maxl);
void ui_fold(const char *in, char *out, size_t n); /* ASCII, lowercase */
int  ui_key_char(int key);                         /* calculator char for a key */
int  ui_edit_number(const char *label, double *v);
int  ui_edit_text(const char *label, char *buf, size_t n);

void fmt_num(double v, char *out, size_t n, int maxdec); /* grouped, trimmed */
void fmt_g(double v, char *out, size_t n);               /* 10 significant digits */

/* calc.c */
extern int g_deg; /* 1 = degrees, 0 = radians */
int    calc_eval(const char *s, double *out);
void   calc_set_ans(double v);
double calc_get_ans(void);

#endif
