#include <math.h>
#include <string.h>
#include "ui.h"

#define PI 3.14159265358979323846

int g_deg = 1;

static const char *p;
static int bad;
static double g_ans = 0;

void calc_set_ans(double v) { g_ans = v; }
double calc_get_ans(void) { return g_ans; }

static int isdig(char c) { return c >= '0' && c <= '9'; }
static void skip(void) { while (*p == ' ') p++; }

static double pw10(int e)
{
    double r = 1;
    int neg = e < 0;
    if (neg) e = -e;
    if (e > 300) { bad = 1; return 1; }
    while (e--) r *= 10.0;
    return neg ? 1.0 / r : r;
}

static double expr(void);
static double unary(void);

static double apply(const char *name, double a)
{
    double k = g_deg ? PI / 180.0 : 1.0;
    if (!strcmp(name, "sin")) return sin(a * k);
    if (!strcmp(name, "cos")) return cos(a * k);
    if (!strcmp(name, "tan")) return tan(a * k);
    if (!strcmp(name, "asin")) { if (a < -1 || a > 1) bad = 1; return asin(a) / k; }
    if (!strcmp(name, "acos")) { if (a < -1 || a > 1) bad = 1; return acos(a) / k; }
    if (!strcmp(name, "atan")) return atan(a) / k;
    if (!strcmp(name, "ln")) { if (a <= 0) bad = 1; return a > 0 ? log(a) : 0; }
    if (!strcmp(name, "log")) { if (a <= 0) bad = 1; return a > 0 ? log10(a) : 0; }
    if (!strcmp(name, "sqrt")) { if (a < 0) bad = 1; return a >= 0 ? sqrt(a) : 0; }
    if (!strcmp(name, "abs")) return fabs(a);
    if (!strcmp(name, "exp")) return exp(a);
    bad = 1;
    return 0;
}

static double primary(void)
{
    skip();
    if (*p == '(') {
        p++;
        double v = expr();
        skip();
        if (*p == ')') p++;          /* a missing ")" at the end is tolerated */
        return v;
    }
    if (isdig(*p) || *p == '.') {
        double mant = 0;
        int dec = 0, any = 0;
        while (isdig(*p)) { mant = mant * 10 + (*p++ - '0'); any = 1; }
        if (*p == '.') {
            p++;
            while (isdig(*p)) { mant = mant * 10 + (*p++ - '0'); dec++; any = 1; }
        }
        if (!any) { bad = 1; return 0; }
        double v = mant / pw10(dec);
        if (*p == 'E') {
            const char *q = p + 1;
            int sg = 1, ex = 0, got = 0;
            if (*q == '-') { sg = -1; q++; } else if (*q == '+') q++;
            while (isdig(*q)) { ex = ex * 10 + (*q++ - '0'); got = 1; if (ex > 400) break; }
            if (got) { p = q; v *= pw10(sg * ex); }
        }
        return v;
    }
    if (*p >= 'a' && *p <= 'z') {
        char name[8];
        int n = 0;
        while (*p >= 'a' && *p <= 'z' && n < 7) name[n++] = *p++;
        name[n] = 0;
        if (!strcmp(name, "pi")) return PI;
        if (!strcmp(name, "e")) return 2.71828182845904523536;
        if (!strcmp(name, "ans")) return g_ans;
        skip();
        if (*p != '(') { bad = 1; return 0; }
        p++;
        double a = expr();
        skip();
        if (*p == ')') p++;
        return apply(name, a);
    }
    bad = 1;
    return 0;
}

static double power(void)
{
    double b = primary();
    skip();
    if (*p == '^') {
        p++;
        double e = unary();
        if (b < 0 && e != floor(e)) { bad = 1; return 0; }
        if (b == 0 && e < 0) { bad = 1; return 0; }
        return pow(b, e);
    }
    return b;
}

static double unary(void)
{
    skip();
    if (*p == '-') { p++; return -unary(); }
    if (*p == '+') { p++; return unary(); }
    return power();
}

static double term(void)
{
    double v = unary();
    for (;;) {
        skip();
        if (*p == '*') { p++; v *= unary(); }
        else if (*p == '/') {
            p++;
            double d = unary();
            if (d == 0) { bad = 1; return 0; }
            v /= d;
        } else if (*p == '(' || (*p >= 'a' && *p <= 'z')) {
            v *= unary();            /* implicit multiplication: 2pi, 3(4+1) */
        } else break;
        if (bad) return 0;
    }
    return v;
}

static double expr(void)
{
    double v = term();
    for (;;) {
        skip();
        if (*p == '+') { p++; v += term(); }
        else if (*p == '-') { p++; v -= term(); }
        else break;
        if (bad) return 0;
    }
    return v;
}

int calc_eval(const char *s, double *out)
{
    p = s;
    bad = 0;
    double v = expr();
    skip();
    if (*p) bad = 1;
    if (bad || v != v || v > 1e300 || v < -1e300) return 1;
    *out = v;
    return 0;
}
