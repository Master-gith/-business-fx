#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "data.h"

#define NCH 6
#define MAXP 13

static const bi_t CH_NAME[NCH] = {
    BI("Break-even chart", "Gráfico de punto de equilibrio"),
    BI("Revenue graph", "Gráfico de ingresos"),
    BI("Total costs graph", "Gráfico de costos totales"),
    BI("Profit and loss graph", "Gráfico de beneficio y pérdida"),
    BI("Cash flow graph", "Gráfico de flujo de caja"),
    BI("Sales growth graph", "Gráfico de crecimiento de ventas"),
};

#define PN_FC BI("Fixed costs (FC)", "Costos fijos (FC)")
#define PN_P  BI("Selling price (P)", "Precio de venta (P)")
#define PN_VC BI("Variable cost per unit (VC)", "Costo variable por unidad (VC)")
#define PN_Q  BI("Max units Q (0 = auto)", "Unidades máx. Q (0 = auto)")

static const bi_t PAR0[4] = { PN_FC, PN_P, PN_VC, PN_Q };
static const bi_t PAR1[2] = { PN_P, PN_Q };
static const bi_t PAR2[3] = { PN_FC, PN_VC, PN_Q };
static const bi_t PAR3[4] = { PN_P, PN_VC, PN_FC, PN_Q };

static const int CH_NP[NCH] = { 4, 2, 3, 4, 13, 6 };
static const double CH_DEF[NCH][MAXP] = {
    { 1000, 20, 10, 0 },
    { 20, 200 },
    { 1000, 10, 200 },
    { 20, 10, 1000, 200 },
    { 500, 800, 900, 700, 1000, 950, 1100, 1000, 850, 900, 1200, 1000, 1000 },
    { 100, 120, 150, 140, 180, 210 },
};

static double V[NCH][MAXP];
static int inited = 0;

static void init(void)
{
    if (inited) return;
    for (int c = 0; c < NCH; c++)
        for (int i = 0; i < MAXP; i++) V[c][i] = CH_DEF[c][i];
    inited = 1;
}

int chart_count(void) { return NCH; }
bi_t chart_name(int c) { return CH_NAME[c]; }
int chart_nparams(int c) { return CH_NP[c]; }
double *chart_values(int c) { init(); return V[c]; }

void chart_param_label(int c, int p, char *buf, size_t n)
{
    const bi_t *t = NULL;
    switch (c) {
    case 0: t = PAR0; break;
    case 1: t = PAR1; break;
    case 2: t = PAR2; break;
    case 3: t = PAR3; break;
    }
    if (t) { snprintf(buf, n, "%s", tr(t[p])); return; }
    if (c == 4) {
        if (p == 0) snprintf(buf, n, "%s", g_lang ? "Saldo inicial" : "Opening balance");
        else if (p <= 6) snprintf(buf, n, g_lang ? "Entradas M%d" : "Inflow M%d", p);
        else snprintf(buf, n, g_lang ? "Salidas M%d" : "Outflow M%d", p - 6);
    } else {
        snprintf(buf, n, g_lang ? "Ventas P%d" : "Sales P%d", p + 1);
    }
}

/* ------------------------------------------------------------------ *
 * Plot area and axes                                                 *
 * ------------------------------------------------------------------ */
static int PX0 = 50, PX1 = 388, PY0 = 38, PY1 = 178;
static double gx0, gx1, gy0, gy1;

static int mapx(double x) { return PX0 + (int)floor((x - gx0) / (gx1 - gx0) * (PX1 - PX0) + 0.5); }
static int mapy(double y) { return PY1 - (int)floor((y - gy0) / (gy1 - gy0) * (PY1 - PY0) + 0.5); }
static int cy(int y) { return y < PY0 ? PY0 : (y > PY1 ? PY1 : y); }

static double nice_step(double range, int n)
{
    double raw = range / n, mag = 1, f;
    if (!(raw > 0) || raw > 1e300) return 1;
    while (mag * 10 <= raw) mag *= 10;
    while (mag > raw) mag /= 10;
    f = raw / mag;
    return (f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10) * mag;
}

static void thick(int x0, int y0, int x1, int y1, int col)
{
    dline(x0, y0, x1, y1, col);
    dline(x0, y0 + 1, x1, y1 + 1, col);
}

static void dashed_h(int x0, int x1, int y, int col)
{
    for (int x = x0; x < x1; x += 6) dline(x, y, x + 2 < x1 ? x + 2 : x1, y, col);
}

static void dashed_v(int x, int y0, int y1, int col)
{
    int a = y0 < y1 ? y0 : y1, b = y0 < y1 ? y1 : y0;
    for (int y = a; y < b; y += 6) dline(x, y, x, y + 2 < b ? y + 2 : b, col);
}

static void dot(int x, int y, int col) { drect(x - 2, y - 2, x + 2, y + 2, col); }

static void label_at(int x, int y, int col, const char *s)
{   /* keep the label inside the plot area */
    int w = ui_w(s);
    if (x + w > SCR_W - 2) x = SCR_W - 2 - w;
    if (x < 2) x = 2;
    if (y < PY0 - g_th) y = PY0 - g_th;
    if (y > PY1 - g_th) y = PY1 - g_th;
    ui_text(x, y, col, s);
}

static void setup(void)
{
    PY0 = HEAD_H + 2 * g_lh + 6;
    PY1 = FOOT_Y - 2 * g_lh - 4;
}

/* Axes with grid. xcat = 1: caller draws x labels (categories). */
static void frame(double xmin, double xmax, double ymin, double ymax, int xcat,
                  const char *xlabel, const char *ylabel)
{
    char b[40];
    if (!(xmax > xmin)) xmax = xmin + 1;
    if (!(ymax > ymin)) ymax = ymin + 1;
    double ys = nice_step(ymax - ymin, 5);
    gy0 = floor(ymin / ys) * ys;
    gy1 = ceil(ymax / ys) * ys;
    if (gy1 <= gy0) gy1 = gy0 + ys;
    gx0 = xmin;
    gx1 = xmax;
    for (int i = 0; i <= 24; i++) {
        double y = gy0 + i * ys;
        if (y > gy1 + ys * 0.001) break;
        int py = mapy(y);
        dline(PX0, py, PX1, py, COL_GRID);
        fmt_g(y, b, sizeof b);
        ui_text_r(PX0 - 4, py - g_th / 2, COL_TXT, b);
    }
    if (!xcat) {
        double xs = nice_step(xmax - xmin, 6);
        double t0 = ceil(xmin / xs) * xs;
        for (int i = 0; i <= 24; i++) {
            double x = t0 + i * xs;
            if (x > xmax + xs * 0.001) break;
            int px = mapx(x);
            dline(px, PY0, px, PY1, COL_GRID);
            fmt_g(x, b, sizeof b);
            ui_text(px - ui_w(b) / 2, PY1 + 3, COL_TXT, b);
        }
        if (xlabel) ui_text_r(PX1, PY1 + 3 + g_lh, COL_MUTE, xlabel);
    }
    if (gy0 < 0 && gy1 > 0) dline(PX0, mapy(0), PX1, mapy(0), COL_TXT);
    dline(PX0, PY0, PX0, PY1, COL_TXT);
    dline(PX0, PY1, PX1, PY1, COL_TXT);
    if (ylabel) ui_text(2, HEAD_H + 2 + g_lh, COL_MUTE, ylabel);
}

static int legend(int x, int col, const char *s)
{
    int y = HEAD_H + 2;
    drect(x, y + g_th / 2 - 1, x + 9, y + g_th / 2 + 1, col);
    ui_text(x + 13, y, COL_TXT, s);
    return x + 13 + ui_w(s) + 8;
}

static const char *money(void) { return g_lang ? "Unidades monetarias" : "Currency units"; }
static const char *unitsx(void) { return g_lang ? "Unidades (Q)" : "Units (Q)"; }

static void end_label(double v, const char *name, int col, int x, int y)
{
    char b[40], t[60];
    fmt_num(v, b, sizeof b, 0);
    snprintf(t, sizeof t, "%s=%s", name, b);
    label_at(x - ui_w(t) - 3, y - g_th - 2, col, t);
}

/* ------------------------------------------------------------------ *
 * 0: break-even chart                                                *
 * ------------------------------------------------------------------ */
static void draw_bep(void)
{
    double FC = V[0][0], P = V[0][1], VC = V[0][2], Q = V[0][3];
    double cu = P - VC, bep = cu > 0 ? FC / cu : -1;
    char b[40], t[90];
    if (Q <= 0) Q = bep > 0 ? bep * 2 : 100;
    double top = fmax(P * Q, FC + VC * Q);
    frame(0, Q, 0, top, 0, unitsx(), money());
    for (int px = PX0 + 1; px <= PX1; px++) {
        double x = gx0 + (double)(px - PX0) * (gx1 - gx0) / (PX1 - PX0);
        double tr_ = P * x, tc = FC + VC * x;
        dline(px, cy(mapy(tr_)), px, cy(mapy(tc)), tr_ >= tc ? COL_PROFIT : COL_LOSS);
    }
    dashed_h(PX0, PX1, cy(mapy(FC)), COL_FC);
    thick(mapx(0), cy(mapy(FC)), mapx(Q), cy(mapy(FC + VC * Q)), COL_TC);
    thick(mapx(0), mapy(0), mapx(Q), cy(mapy(P * Q)), COL_TR);
    int x = legend(PX0 - 40, COL_TR, "TR");
    x = legend(x, COL_TC, "TC");
    x = legend(x, COL_FC, "FC");
    x = legend(x, COL_PROFIT, g_lang ? "Beneficio" : "Profit");
    legend(x, COL_LOSS, g_lang ? "Pérdida" : "Loss");
    if (bep > 0 && bep <= Q) {
        int bx = mapx(bep), by = cy(mapy(P * bep));
        dashed_v(bx, by, PY1, COL_TXT);
        dashed_h(PX0, bx, by, COL_TXT);
        dot(bx, by, C_BLACK);
        fmt_num(bep, b, sizeof b, 2);
        char r[40];
        fmt_num(P * bep, r, sizeof r, 2);
        snprintf(t, sizeof t, "BEP (%s; %s)", b, r);
        label_at(bx + 5, by + 4, COL_TXT, t);
    } else {
        ui_text(PX0 + 8, PY0 + 4, COL_BAD, g_lang ? "Sin punto de equilibrio" : "No break-even point");
    }
}

/* 1: revenue */
static void draw_rev(void)
{
    double P = V[1][0], Q = V[1][1];
    if (Q <= 0) Q = 100;
    double top = P * Q > 0 ? P * Q : 1;
    frame(0, Q, 0, top, 0, unitsx(), money());
    thick(mapx(0), mapy(0), mapx(Q), cy(mapy(P * Q)), COL_TR);
    legend(PX0 - 40, COL_TR, g_lang ? "IT = P x Q" : "TR = P x Q");
    int x1 = mapx(Q), y1 = cy(mapy(P * Q));
    dot(x1, y1, C_BLACK);
    end_label(P * Q, "TR", COL_TR, x1, y1);
}

/* 2: total costs */
static void draw_costs(void)
{
    double FC = V[2][0], VC = V[2][1], Q = V[2][2];
    if (Q <= 0) Q = 100;
    double top = FC + VC * Q > 0 ? FC + VC * Q : 1;
    frame(0, Q, 0, top, 0, unitsx(), money());
    dashed_h(PX0, PX1, cy(mapy(FC)), COL_FC);
    thick(mapx(0), mapy(0), mapx(Q), cy(mapy(VC * Q)), COL_GOOD);
    thick(mapx(0), cy(mapy(FC)), mapx(Q), cy(mapy(FC + VC * Q)), COL_TC);
    int x = legend(PX0 - 40, COL_FC, "FC");
    x = legend(x, COL_GOOD, "TVC");
    legend(x, COL_TC, "TC = FC + TVC");
    end_label(FC + VC * Q, "TC", COL_TC, mapx(Q), cy(mapy(FC + VC * Q)));
    end_label(VC * Q, "TVC", COL_GOOD, mapx(Q), cy(mapy(VC * Q)) + 2 * g_th);
}

/* 3: profit and loss */
static void draw_pl(void)
{
    double P = V[3][0], VC = V[3][1], FC = V[3][2], Q = V[3][3];
    double cu = P - VC, bep = cu > 0 ? FC / cu : -1;
    if (Q <= 0) Q = bep > 0 ? bep * 2 : 100;
    double pe = cu * Q - FC;
    frame(0, Q, fmin(-FC, pe), fmax(0, pe), 0, unitsx(), money());
    int y0 = mapy(0);
    for (int px = PX0 + 1; px <= PX1; px++) {
        double x = gx0 + (double)(px - PX0) * (gx1 - gx0) / (PX1 - PX0);
        double pr = cu * x - FC;
        dline(px, cy(y0), px, cy(mapy(pr)), pr >= 0 ? COL_PROFIT : COL_LOSS);
    }
    dline(PX0, cy(y0), PX1, cy(y0), COL_TXT);
    thick(mapx(0), cy(mapy(-FC)), mapx(Q), cy(mapy(pe)), COL_TR);
    int x = legend(PX0 - 40, COL_TR, g_lang ? "Beneficio = IT - CT" : "Profit = TR - TC");
    x = legend(x, COL_PROFIT, g_lang ? "Beneficio" : "Profit");
    legend(x, COL_LOSS, g_lang ? "Pérdida" : "Loss");
    end_label(pe, g_lang ? "B" : "P", COL_TR, mapx(Q), cy(mapy(pe)));
    if (bep > 0 && bep <= Q) {
        char b[40], t[60];
        fmt_num(bep, b, sizeof b, 2);
        snprintf(t, sizeof t, "BEP = %s", b);
        dot(mapx(bep), cy(y0), C_BLACK);
        label_at(mapx(bep) + 5, cy(y0) + 3, COL_TXT, t);
    }
}

/* 4: cash flow */
static void draw_cash(void)
{
    double net[6], cl[6], open = V[4][0], mn = 0, mx = 0, run = open;
    char b[40], t[16];
    for (int i = 0; i < 6; i++) {
        net[i] = V[4][1 + i] - V[4][7 + i];
        run += net[i];
        cl[i] = run;
        mn = fmin(mn, fmin(net[i], cl[i]));
        mx = fmax(mx, fmax(net[i], cl[i]));
    }
    frame(0, 6, mn, mx * 1.12, 1, NULL, money());
    double slot = (double)(PX1 - PX0) / 6.0;
    int y0 = mapy(0), px[6], py[6];
    for (int i = 0; i < 6; i++) {
        int xc = PX0 + (int)((i + 0.5) * slot), hw = (int)(slot * 0.22);
        int yv = cy(mapy(net[i]));
        drect(xc - hw, yv < y0 ? yv : y0, xc + hw, yv < y0 ? y0 : yv, net[i] >= 0 ? COL_GOOD : COL_BAD);
        snprintf(t, sizeof t, "M%d", i + 1);
        ui_text(xc - ui_w(t) / 2, PY1 + 3, COL_TXT, t);
        px[i] = xc;
        py[i] = cy(mapy(cl[i]));
    }
    for (int i = 0; i < 5; i++) thick(px[i], py[i], px[i + 1], py[i + 1], COL_TR);
    for (int i = 0; i < 6; i++) {
        dot(px[i], py[i], COL_TR);
        fmt_num(cl[i], b, sizeof b, 0);
        label_at(px[i] - ui_w(b) / 2, py[i] - g_th - 4, COL_TR, b);
    }
    int x = legend(PX0 - 40, COL_GOOD, g_lang ? "Flujo neto" : "Net cash flow");
    legend(x, COL_TR, g_lang ? "Saldo final" : "Closing balance");
    ui_text_r(PX1, PY1 + 3 + g_lh, COL_MUTE, g_lang ? "M = mes" : "M = month");
}

/* 5: sales growth */
static void draw_growth(void)
{
    double mx = 1, mn = 0;
    char b[40], t[24];
    for (int i = 0; i < 6; i++) { mx = fmax(mx, V[5][i]); mn = fmin(mn, V[5][i]); }
    frame(0, 6, mn, mx * 1.12, 1, NULL, g_lang ? "Ventas" : "Sales");
    double slot = (double)(PX1 - PX0) / 6.0;
    int y0 = mapy(0);
    for (int i = 0; i < 6; i++) {
        int xc = PX0 + (int)((i + 0.5) * slot), hw = (int)(slot * 0.25);
        int yv = cy(mapy(V[5][i]));
        drect(xc - hw, yv < y0 ? yv : y0, xc + hw, yv < y0 ? y0 : yv, COL_TR);
        fmt_num(V[5][i], b, sizeof b, 0);
        label_at(xc - ui_w(b) / 2, yv - g_th - 3, COL_TXT, b);
        snprintf(t, sizeof t, "P%d", i + 1);
        ui_text(xc - ui_w(t) / 2, PY1 + 3, COL_TXT, t);
        if (i > 0 && V[5][i - 1] != 0) {
            double g = (V[5][i] - V[5][i - 1]) / V[5][i - 1] * 100.0;
            fmt_num(g, b, sizeof b, 1);
            snprintf(t, sizeof t, "%s%s%%", g >= 0 ? "+" : "", b);
            ui_text(xc - ui_w(t) / 2, PY1 + 3 + g_lh, g >= 0 ? COL_GOOD : COL_BAD, t);
        } else {
            ui_text(xc - 3, PY1 + 3 + g_lh, COL_MUTE, "-");
        }
    }
    legend(PX0 - 40, COL_TR, g_lang ? "Ventas (P = periodo, % = crecimiento)" : "Sales (P = period, % = growth)");
}

void chart_draw(int c)
{
    init();
    setup();
    switch (c) {
    case 0: draw_bep(); break;
    case 1: draw_rev(); break;
    case 2: draw_costs(); break;
    case 3: draw_pl(); break;
    case 4: draw_cash(); break;
    default: draw_growth(); break;
    }
}
