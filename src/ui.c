#include <stdio.h>
#include <string.h>
#include "ui.h"

int g_lang = 0;
int g_lh = 12;
int g_th = 9;

/* ------------------------------------------------------------------ *
 * Text preparation. Source strings are UTF-8 (accents allowed). The  *
 * default gint font is ASCII-only, so accents are stripped at draw   *
 * time ("Fórmulas" -> "Formulas").                                   *
 * ------------------------------------------------------------------ */
static const char LATIN1[] =
    "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTs"
    "aaaaaaaceeeeiiiidnooooo/ouuuuyty";

static void prep(const char *s, char *o, size_t n)
{
    size_t k = 0;
    while (*s && k + 1 < n) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x80) {
            o[k++] = (char)c; s++;
        } else if (c == 0xC3 && (unsigned char)s[1] >= 0x80 && (unsigned char)s[1] < 0xC0) {
            o[k++] = LATIN1[(unsigned char)s[1] - 0x80]; s += 2;
        } else if (c == 0xC2 && s[1]) {
            unsigned char d = (unsigned char)s[1];
            o[k++] = d == 0xBF ? '?' : d == 0xA1 ? '!' : d == 0xB0 ? 'o' : '?';
            s += 2;
        } else if (c == 0xE2 && s[1] && s[2]) {
            unsigned char d = (unsigned char)s[1], e = (unsigned char)s[2];
            char r = '?';
            if (d == 0x88 && e == 0x92) r = '-';
            else if (d == 0x80 && (e == 0x93 || e == 0x94)) r = '-';
            else if (d == 0x80 && (e == 0x98 || e == 0x99)) r = '\'';
            else if (d == 0x80 && (e == 0x9C || e == 0x9D)) r = '"';
            o[k++] = r; s += 3;
        } else {
            o[k++] = '?'; s++;
            while (((unsigned char)*s & 0xC0) == 0x80) s++;
        }
    }
    o[k] = 0;
}

void ui_fold(const char *in, char *out, size_t n)
{
    prep(in, out, n);
    for (char *p = out; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
}

void ui_init(void)
{
    int h = 9;
    dsize("Ag", NULL, NULL, &h);
    g_th = h;
    g_lh = h + 3;
}

int ui_w(const char *s)
{
    char b[256];
    int w = 0;
    prep(s, b, sizeof b);
    dsize(b, NULL, &w, NULL);
    return w;
}

void ui_text(int x, int y, int color, const char *s)
{
    char b[256];
    prep(s, b, sizeof b);
    dtext(x, y, color, b);
}

void ui_text_r(int xr, int y, int color, const char *s)
{
    ui_text(xr - ui_w(s), y, color, s);
}

void ui_header(const char *title)
{
    int ty = (HEAD_H - g_th) / 2;
    drect(0, 0, SCR_W - 1, HEAD_H - 1, COL_ACC);
    ui_text(5, ty, C_WHITE, title);
    ui_text_r(SCR_W - 5, ty, C_WHITE, g_lang ? "ES" : "EN");
}

void ui_footer(const char *const f[6])
{
    int w = SCR_W / 6;
    for (int i = 0; i < 6; i++) {
        int x0 = i * w + 1, x1 = x0 + w - 3;
        if (!f[i]) continue;
        drect(x0, FOOT_Y, x1, SCR_H - 1, COL_ACC);
        int tw = ui_w(f[i]);
        ui_text(x0 + (x1 - x0 - tw) / 2, FOOT_Y + (SCR_H - FOOT_Y - g_th) / 2, C_WHITE, f[i]);
    }
}

/* ------------------------------------------------------------------ *
 * Word wrap. Lines point into a static buffer (valid until next call)*
 * ------------------------------------------------------------------ */
static char wbuf[1700];

static int raw_w(const char *s)
{
    int w = 0;
    dsize(s, NULL, &w, NULL);
    return w;
}

int ui_wrap(const char *s, int maxw, const char **lines, int maxl)
{
    prep(s, wbuf, sizeof wbuf);
    int n = 0;
    char *p = wbuf;
    while (*p && n < maxl) {
        lines[n] = p;
        char *last_space = NULL, *q = p;
        for (;;) {
            char *w = q;
            while (*w && *w != ' ' && *w != '\n') w++;
            char save = *w;
            *w = 0;
            int width = raw_w(p);
            *w = save;
            if (width > maxw && last_space) {
                *last_space = 0;
                p = last_space + 1;
                break;
            }
            if (save == '\n') { *w = 0; p = w + 1; break; }
            if (save == 0) { p = w; break; }
            last_space = w;
            q = w + 1;
        }
        n++;
    }
    return n;
}

void ui_msg(const char *s)
{
    const char *lines[6];
    int n = ui_wrap(s, 290, lines, 6);
    int h = n * g_lh + 34, y0 = (SCR_H - h) / 2;
    drect(40, y0, SCR_W - 41, y0 + h, C_WHITE);
    dline(40, y0, SCR_W - 41, y0, COL_ACC);
    dline(40, y0 + h, SCR_W - 41, y0 + h, COL_ACC);
    dline(40, y0, 40, y0 + h, COL_ACC);
    dline(SCR_W - 41, y0, SCR_W - 41, y0 + h, COL_ACC);
    for (int i = 0; i < n; i++) ui_text(52, y0 + 8 + i * g_lh, COL_TXT, lines[i]);
    ui_text(52, y0 + h - g_lh - 2, COL_MUTE, g_lang ? "Pulsa una tecla" : "Press any key");
    dupdate();
    getkey();
}

/* ------------------------------------------------------------------ *
 * Number formatting                                                  *
 * ------------------------------------------------------------------ */
void fmt_num(double v, char *out, size_t n, int maxdec)
{
    static const unsigned long long P10[5] = {1, 10, 100, 1000, 10000};
    char t[48];
    int k = 0;
    if (v != v) { snprintf(out, n, "-"); return; }
    int neg = v < 0;
    if (neg) v = -v;
    if (v > 1e14) v = 1e14;
    if (maxdec < 0) maxdec = 0;
    if (maxdec > 4) maxdec = 4;
    unsigned long long sc = (unsigned long long)(v * (double)P10[maxdec] + 0.5);
    unsigned long long ip = sc / P10[maxdec], fp = sc % P10[maxdec];
    char d[24];
    int nd = 0;
    if (ip == 0) d[nd++] = '0';
    while (ip) { d[nd++] = (char)('0' + ip % 10); ip /= 10; }
    if (neg && sc) t[k++] = '-';
    for (int i = nd - 1; i >= 0; i--) {
        t[k++] = d[i];
        if (i > 0 && i % 3 == 0) t[k++] = g_lang ? '.' : ',';
    }
    if (fp && maxdec) {
        char f[6];
        for (int i = maxdec - 1; i >= 0; i--) { f[i] = (char)('0' + fp % 10); fp /= 10; }
        int len = maxdec;
        while (len > 0 && f[len - 1] == '0') len--;
        t[k++] = g_lang ? ',' : '.';
        memcpy(t + k, f, (size_t)len);
        k += len;
    }
    t[k] = 0;
    snprintf(out, n, "%s", t);
}

void fmt_g(double v, char *out, size_t n)
{
    char t[48];
    int k = 0;
    char dec = g_lang ? ',' : '.';
    if (v != v) { snprintf(out, n, "ERR"); return; }
    if (v > 1e300 || v < -1e300) { snprintf(out, n, v < 0 ? "-INF" : "INF"); return; }
    if (v == 0) { snprintf(out, n, "0"); return; }
    int neg = v < 0;
    if (neg) v = -v;
    int e = 0;
    double m = v;
    while (m >= 10.0) { m /= 10.0; e++; }
    while (m < 1.0) { m *= 10.0; e--; }
    long long mi = (long long)(m * 1e9 + 0.5);
    if (mi >= 10000000000LL) { mi = 1000000000LL; e++; }
    char d[11];
    for (int i = 9; i >= 0; i--) { d[i] = (char)('0' + mi % 10); mi /= 10; }
    d[10] = 0;
    int len = 10;
    while (len > 1 && d[len - 1] == '0') len--;
    if (neg) t[k++] = '-';
    if (e >= 10 || e < -4) {
        t[k++] = d[0];
        if (len > 1) { t[k++] = dec; memcpy(t + k, d + 1, (size_t)(len - 1)); k += len - 1; }
        k += snprintf(t + k, sizeof t - (size_t)k, "E%d", e);
    } else if (e >= 0) {
        for (int i = 0; i <= e; i++) t[k++] = d[i];
        if (len > e + 1) { t[k++] = dec; memcpy(t + k, d + e + 1, (size_t)(len - e - 1)); k += len - e - 1; }
    } else {
        t[k++] = '0'; t[k++] = dec;
        for (int i = 0; i < -e - 1; i++) t[k++] = '0';
        memcpy(t + k, d, (size_t)len);
        k += len;
    }
    t[k] = 0;
    snprintf(out, n, "%s", t);
}

/* ------------------------------------------------------------------ *
 * Keys                                                               *
 * ------------------------------------------------------------------ */
int ui_key_char(int k)
{
    switch (k) {
    case KEY_0: return '0'; case KEY_1: return '1'; case KEY_2: return '2';
    case KEY_3: return '3'; case KEY_4: return '4'; case KEY_5: return '5';
    case KEY_6: return '6'; case KEY_7: return '7'; case KEY_8: return '8';
    case KEY_9: return '9'; case KEY_DOT: return '.';
    case KEY_ADD: return '+'; case KEY_SUB: return '-'; case KEY_NEG: return '-';
    case KEY_MUL: return '*'; case KEY_DIV: return '/';
    case KEY_LEFTP: return '('; case KEY_RIGHTP: return ')';
    case KEY_POWER: return '^'; case KEY_EXP: return 'E';
    default: return 0;
    }
}

/* Letters printed in red above the keys (fx-CG50 alpha layout) */
static int alpha_char(int k)
{
    switch (k) {
    case KEY_XOT: return 'a'; case KEY_LOG: return 'b'; case KEY_LN: return 'c';
    case KEY_SIN: return 'd'; case KEY_COS: return 'e'; case KEY_TAN: return 'f';
    case KEY_FRAC: return 'g'; case KEY_FD: return 'h'; case KEY_LEFTP: return 'i';
    case KEY_RIGHTP: return 'j'; case KEY_COMMA: return 'k'; case KEY_ARROW: return 'l';
    case KEY_7: return 'm'; case KEY_8: return 'n'; case KEY_9: return 'o';
    case KEY_4: return 'p'; case KEY_5: return 'q'; case KEY_6: return 'r';
    case KEY_MUL: return 's'; case KEY_DIV: return 't';
    case KEY_1: return 'u'; case KEY_2: return 'v'; case KEY_3: return 'w';
    case KEY_ADD: return 'x'; case KEY_SUB: return 'y'; case KEY_0: return 'z';
    case KEY_DOT: return ' ';
    default: return 0;
    }
}

static void draw_box(int x0, int y0, int x1, int y1, int col)
{
    dline(x0, y0, x1, y0, col); dline(x0, y1, x1, y1, col);
    dline(x0, y0, x0, y1, col); dline(x1, y0, x1, y1, col);
}

static void show_expr(const char *buf, int x, int y)
{
    char d[64];
    size_t i = 0;
    for (; buf[i] && i < sizeof d - 3; i++) d[i] = buf[i] == '*' ? 'x' : buf[i];
    d[i++] = '_';
    d[i] = 0;
    ui_text(x, y, COL_TXT, d);
}

int ui_edit_number(const char *label, double *v)
{
    char buf[48] = "";
    size_t len = 0;
    int err = 0;
    for (;;) {
        char cur[48];
        dclear(COL_BG);
        ui_header(label);
        if (*v != *v) snprintf(cur, sizeof cur, "-"); else fmt_g(*v, cur, sizeof cur);
        ui_text(10, 28, COL_MUTE, g_lang ? "Valor actual:" : "Current value:");
        ui_text(10, 28 + g_lh, COL_TXT, cur);
        ui_text(10, 28 + 3 * g_lh, COL_MUTE,
                g_lang ? "Nuevo valor (puedes escribir una operacion):"
                       : "New value (you can type an operation):");
        draw_box(10, 28 + 4 * g_lh + 2, SCR_W - 11, 28 + 5 * g_lh + 8, COL_ACC);
        show_expr(buf, 14, 28 + 4 * g_lh + 6);
        if (err) ui_text(10, 28 + 6 * g_lh + 8, COL_BAD, g_lang ? "Operacion no valida" : "Invalid operation");
        ui_text(10, FOOT_Y - g_lh - 2, COL_MUTE, g_lang ? "EXE = aceptar    EXIT = cancelar    DEL = borrar" : "EXE = accept    EXIT = cancel    DEL = delete");
        const char *f[6] = {g_lang ? "Borrar" : "Clear", 0, 0, 0, 0, 0};
        ui_footer(f);
        dupdate();
        int k = getkey().key;
        if (k == KEY_EXIT) return 0;
        if (k == KEY_F1) { len = 0; buf[0] = 0; err = 0; continue; }
        if (k == KEY_DEL) { if (len) buf[--len] = 0; err = 0; continue; }
        if (k == KEY_EXE) {
            double r;
            if (!len) return 0;
            if (calc_eval(buf, &r) == 0) { *v = r; return 1; }
            err = 1;
            continue;
        }
        int c = ui_key_char(k);
        if (c && len < sizeof buf - 1) { buf[len++] = (char)c; buf[len] = 0; err = 0; }
    }
}

int ui_edit_text(const char *label, char *buf, size_t n)
{
    size_t len = strlen(buf);
    for (;;) {
        dclear(COL_BG);
        ui_header(label);
        ui_text(10, 28, COL_MUTE, g_lang ? "Escribe con ALPHA + tecla (letras en rojo)." : "Type with ALPHA + key (red letters).");
        draw_box(10, 28 + 2 * g_lh, SCR_W - 11, 28 + 3 * g_lh + 6, COL_ACC);
        show_expr(buf, 14, 28 + 2 * g_lh + 4);
        ui_text(10, FOOT_Y - g_lh - 2, COL_MUTE, g_lang ? "EXE = buscar    EXIT = volver    DEL = borrar" : "EXE = search    EXIT = back    DEL = delete");
        const char *f[6] = {g_lang ? "Borrar" : "Clear", 0, 0, 0, 0, g_lang ? "IDIOMA ES" : "LANG EN"};
        ui_footer(f);
        dupdate();
        key_event_t ev = getkey();
        int k = ev.key;
        if (k == KEY_EXIT) return 0;
        if (k == KEY_F6) { g_lang ^= 1; continue; }
        if (k == KEY_F1) { len = 0; buf[0] = 0; continue; }
        if (k == KEY_DEL) { if (len) buf[--len] = 0; continue; }
        if (k == KEY_EXE) { if (len) return 1; continue; }
        int c = ev.alpha ? alpha_char(k) : ui_key_char(k);
        if (!ev.alpha && !(c >= '0' && c <= '9')) c = 0;
        if (c && len < n - 1) { buf[len++] = (char)c; buf[len] = 0; }
    }
}
