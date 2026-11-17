#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "data.h"

const bi_t ERRMSG[4] = {
    BI("", ""),
    BI("Cannot divide by zero. Change the values.", "No se puede dividir entre cero. Cambia los valores."),
    BI("The selling price must be higher than the variable cost per unit.",
       "El precio de venta debe ser mayor que el costo variable por unidad."),
    BI("Enter at least one value.", "Introduce al menos un valor."),
};

/* ---- helpers -------------------------------------------------------- */
static void N(char *b, double v) { fmt_num(v, b, 40, 4); } /* inputs: up to 4 decimals */
static void R(char *b, double v) { fmt_num(v, b, 40, 2); } /* results: 2 decimals      */

static void NP(char *b, double v) /* negative numbers get parentheses */
{
    char t[40];
    N(t, v);
    if (v < 0) snprintf(b, 40, "(%s)", t); else snprintf(b, 40, "%s", t);
}

static step_t *nst(result_t *r)
{
    step_t *s = &r->st[r->nsteps < MAXSTEPS ? r->nsteps : MAXSTEPS - 1];
    if (r->nsteps < MAXSTEPS) r->nsteps++;
    s->pre[0] = s->num[0] = s->den[0] = s->post[0] = 0;
    return s;
}

static void stxt(result_t *r, const char *pre, const char *txt)
{
    step_t *s = nst(r);
    snprintf(s->pre, sizeof s->pre, "%s", pre);
    snprintf(s->num, sizeof s->num, "%s", txt);
}

static void sfr(result_t *r, const char *pre, const char *num, const char *den, const char *post)
{
    step_t *s = nst(r);
    snprintf(s->pre, sizeof s->pre, "%s", pre);
    snprintf(s->num, sizeof s->num, "%s", num);
    snprintf(s->den, sizeof s->den, "%s", den);
    snprintf(s->post, sizeof s->post, "%s", post);
}

/* "a + b + c" for the filled (non-NAN) values; returns count, sets *sum */
static int sumtxt(const double *v, int n, char *out, size_t sz, double *sum)
{
    int cnt = 0, cut = 0;
    size_t k = 0;
    out[0] = 0;
    *sum = 0;
    for (int i = 0; i < n; i++) {
        if (v[i] != v[i]) continue;
        *sum += v[i];
        if (!cut) {
            char t[40];
            NP(t, v[i]);
            if (k > 34) {
                k += (size_t)snprintf(out + k, sz - k, " + ...");
                cut = 1;
            } else {
                k += (size_t)snprintf(out + k, sz - k, cnt ? " + %s" : "%s", t);
            }
        }
        cnt++;
    }
    return cnt;
}

/* ---- accounting and finance ---------------------------------------- */
static void c_bep(const double *v, result_t *r)
{
    char a[40], b[40], c[40], d[40], t[90];
    double cu = v[1] - v[2];
    if (cu == 0) { r->err = 1; return; }
    if (cu < 0) { r->err = 2; return; }
    N(a, v[0]); N(b, v[1]); N(c, v[2]); N(d, cu);
    snprintf(t, sizeof t, "(%s - %s)", b, c);
    sfr(r, "BEP =", a, t, "");
    sfr(r, "=", a, d, "");
    r->result = v[0] / cu;
    R(a, r->result);
    stxt(r, "=", a);
}

static void c_ber(const double *v, result_t *r)
{
    char a[40], b[40], c[40], d[40], t[90], u[90];
    double cu = v[1] - v[2], fp = v[0] * v[1];
    if (cu == 0) { r->err = 1; return; }
    if (cu < 0) { r->err = 2; return; }
    N(a, v[0]); N(b, v[1]); N(c, v[2]); N(d, cu);
    snprintf(t, sizeof t, "%s x %s", a, b);
    snprintf(u, sizeof u, "(%s - %s)", b, c);
    sfr(r, "BER =", t, u, "");
    N(a, fp);
    sfr(r, "=", a, d, "");
    r->result = fp / cu;
    R(a, r->result);
    stxt(r, "=", a);
}

static void c_tr(const double *v, result_t *r)
{
    char a[40], b[40], t[90];
    N(a, v[0]); N(b, v[1]);
    snprintf(t, sizeof t, "%s x %s", a, b);
    stxt(r, "TR =", t);
    r->result = v[0] * v[1];
    R(a, r->result);
    stxt(r, "=", a);
}

static void c_tc(const double *v, result_t *r)
{
    char a[40], b[40], c[40], d[40], t[90];
    N(a, v[0]); N(b, v[1]); N(c, v[2]);
    snprintf(t, sizeof t, "%s + %s x %s", a, b, c);
    stxt(r, "TC =", t);
    R(d, v[1] * v[2]);
    snprintf(t, sizeof t, "%s + %s", a, d);
    stxt(r, "=", t);
    r->result = v[0] + v[1] * v[2];
    R(a, r->result);
    stxt(r, "=", a);
}

static void c_fc(const double *v, result_t *r)
{
    char a[40], b[40], t[90];
    N(a, v[0]); NP(b, v[1]);
    snprintf(t, sizeof t, "%s - %s", a, b);
    stxt(r, "FC =", t);
    r->result = v[0] - v[1];
    R(a, r->result);
    stxt(r, "=", a);
}

static void c_tvc(const double *v, result_t *r)
{
    char a[40], b[40], t[90];
    N(a, v[0]); N(b, v[1]);
    snprintf(t, sizeof t, "%s x %s", a, b);
    stxt(r, "TVC =", t);
    r->result = v[0] * v[1];
    R(a, r->result);
    stxt(r, "=", a);
}

static void c_profit(const double *v, result_t *r)
{   /* v: P, Q, FC, VC */
    char p[40], q[40], f[40], c[40], tr_[40], tc_[40], vq[40], t[110];
    double TR = v[0] * v[1], VQ = v[3] * v[1], TC = v[2] + VQ;
    N(p, v[0]); N(q, v[1]); N(f, v[2]); N(c, v[3]);
    snprintf(t, sizeof t, "(%s x %s) - (%s + %s x %s)", p, q, f, c, q);
    stxt(r, "Profit =", t);
    R(vq, VQ);
    R(tr_, TR);
    snprintf(t, sizeof t, "%s - (%s + %s)", tr_, f, vq);
    stxt(r, "=", t);
    R(tc_, TC);
    snprintf(t, sizeof t, "%s - %s", tr_, tc_);
    stxt(r, "=", t);
    r->result = TR - TC;
    R(t, r->result);
    stxt(r, "=", t);
    r->msel = r->result > 0 ? 0 : (r->result < 0 ? 1 : 2);
}

static void c_cpu(const double *v, result_t *r)
{
    char a[40], b[40], t[90];
    N(a, v[0]); NP(b, v[1]);
    snprintf(t, sizeof t, "%s - %s", a, b);
    stxt(r, "c =", t);
    r->result = v[0] - v[1];
    R(a, r->result);
    stxt(r, "=", a);
    r->msel = r->result < 0;
}

static void c_contrib(const double *v, result_t *r)
{
    char a[40], b[40], c[40], d[40], t[90];
    double cu = v[0] - v[1];
    N(a, v[0]); NP(b, v[1]); N(c, v[2]); N(d, cu);
    snprintf(t, sizeof t, "(%s - %s) x %s", a, b, c);
    stxt(r, "C =", t);
    snprintf(t, sizeof t, "%s x %s", d, c);
    stxt(r, "=", t);
    r->result = cu * v[2];
    R(a, r->result);
    stxt(r, "=", a);
    r->msel = r->result < 0;
}

static void c_avgc(const double *v, result_t *r)
{
    char a[40], b[40];
    if (v[1] == 0) { r->err = 1; return; }
    N(a, v[0]); N(b, v[1]);
    sfr(r, "AC =", a, b, "");
    r->result = v[0] / v[1];
    R(a, r->result);
    stxt(r, "=", a);
}

static void c_avgr(const double *v, result_t *r)
{
    char a[40], b[40];
    if (v[1] == 0) { r->err = 1; return; }
    N(a, v[0]); N(b, v[1]);
    sfr(r, "AR =", a, b, "");
    r->result = v[0] / v[1];
    R(a, r->result);
    stxt(r, "=", a);
}

static void c_cf(const double *v, result_t *r)
{
    char a[40], b[40], t[90];
    N(a, v[0]); NP(b, v[1]);
    snprintf(t, sizeof t, "%s - %s", a, b);
    stxt(r, "CF =", t);
    r->result = v[0] - v[1];
    R(a, r->result);
    stxt(r, "=", a);
    r->msel = r->result > 0 ? 0 : (r->result < 0 ? 1 : 2);
}

static void c_ncf(const double *v, result_t *r)
{   /* sales receipts, other inflows, payments, other outflows */
    char a[40], b[40], c[40], d[40], t[110];
    double in = v[0] + v[1], out = v[2] + v[3];
    N(a, v[0]); N(b, v[1]); N(c, v[2]); N(d, v[3]);
    snprintf(t, sizeof t, "(%s + %s) - (%s + %s)", a, b, c, d);
    stxt(r, "NCF =", t);
    R(a, in); R(b, out);
    snprintf(t, sizeof t, "%s - %s", a, b);
    stxt(r, "=", t);
    r->result = in - out;
    R(a, r->result);
    stxt(r, "=", a);
    r->msel = r->result > 0 ? 0 : (r->result < 0 ? 1 : 2);
}

static void c_ccb(const double *v, result_t *r)
{   /* opening, inflows, outflows */
    char a[40], b[40], c[40], d[40], t[90];
    double ncf = v[1] - v[2];
    N(a, v[0]); N(b, v[1]); N(c, v[2]);
    snprintf(t, sizeof t, "%s + (%s - %s)", a, b, c);
    stxt(r, "CCB =", t);
    NP(d, ncf);
    snprintf(t, sizeof t, "%s + %s", a, d);
    stxt(r, "=", t);
    r->result = v[0] + ncf;
    R(a, r->result);
    stxt(r, "=", a);
    r->msel = r->result < 0;
}

/* ---- market research and business ---------------------------------- */
static void c_ms(const double *v, result_t *r)
{
    char a[40], b[40], t[90];
    double q;
    if (v[1] == 0) { r->err = 1; return; }
    q = v[0] / v[1];
    N(a, v[0]); N(b, v[1]);
    sfr(r, "MS =", a, b, "x 100");
    N(a, q);
    snprintf(t, sizeof t, "%s x 100", a);
    stxt(r, "=", t);
    r->result = q * 100.0;
    R(a, r->result);
    snprintf(t, sizeof t, "%s%%", a);
    stxt(r, "=", t);
}

static void pct_steps(result_t *r, const char *lhs, double num, double a, double b, double old, const char *nlbl)
{   /* shows (a - b) / old x 100 */
    char s1[40], s2[40], s3[40], t[90], u[90];
    (void)nlbl;
    NP(s1, a); NP(s2, b); N(s3, old);
    snprintf(t, sizeof t, "%s - %s", s1, s2);
    sfr(r, lhs, t, s3, "x 100");
    N(t, num);
    sfr(r, "=", t, s3, "x 100");
    N(u, num / old);
    snprintf(t, sizeof t, "%s x 100", u);
    stxt(r, "=", t);
    R(u, num / old * 100.0);
    snprintf(t, sizeof t, "%s%%", u);
    stxt(r, "=", t);
}

static void c_pc(const double *v, result_t *r)
{   /* old, new */
    if (v[0] == 0) { r->err = 1; return; }
    pct_steps(r, "%chg =", v[1] - v[0], v[1], v[0], v[0], "");
    r->result = (v[1] - v[0]) / v[0] * 100.0;
    r->msel = r->result > 0 ? 0 : (r->result < 0 ? 1 : 2);
}

static void c_pi(const double *v, result_t *r)
{
    if (v[0] == 0) { r->err = 1; return; }
    pct_steps(r, "%inc =", v[1] - v[0], v[1], v[0], v[0], "");
    r->result = (v[1] - v[0]) / v[0] * 100.0;
    r->msel = r->result >= 0 ? 0 : 1;
}

static void c_pd(const double *v, result_t *r)
{
    if (v[0] == 0) { r->err = 1; return; }
    pct_steps(r, "%dec =", v[0] - v[1], v[0], v[1], v[0], "");
    r->result = (v[0] - v[1]) / v[0] * 100.0;
    r->msel = r->result >= 0 ? 0 : 1;
}

static void c_avg(const double *v, result_t *r)
{
    char s[100], a[40], b[40];
    double sum;
    int n = sumtxt(v, 6, s, sizeof s, &sum);
    if (n == 0) { r->err = 3; return; }
    N(b, (double)n);
    sfr(r, "Avg =", s, b, "");
    N(a, sum);
    sfr(r, "=", a, b, "");
    r->result = sum / n;
    R(a, r->result);
    stxt(r, "=", a);
}

static void c_ts(const double *v, result_t *r)
{
    char s[100], a[40];
    double sum;
    int n = sumtxt(v, 6, s, sizeof s, &sum);
    if (n == 0) { r->err = 3; return; }
    stxt(r, "TS =", s);
    r->result = sum;
    R(a, sum);
    stxt(r, "=", a);
}

/* ---- table ------------------------------------------------------------ */
#define MONEY  BI("currency units", "unidades monetarias")
#define UNITS  BI("units", "unidades")
#define PCT    BI("%", "%")
#define NOUNIT BI("", "")
#define NOMEAN BI("", "")

const formula_t FORMULAS[] = {
/* 0 */ { BI("Break-even Point (units)", "Punto de equilibrio (unidades)"),
    "BEP", "FC", "P - VC", 0, 3,
    { { BI("Fixed costs", "Costos fijos"), "FC", 1000, 0 },
      { BI("Selling price per unit", "Precio de venta por unidad"), "P", 20, 0 },
      { BI("Variable cost per unit", "Costo variable por unidad"), "VC", 10, 0 } },
    UNITS,
    { BI("The company must sell %s units to cover all its costs.",
         "La empresa debe vender %s unidades para cubrir todos sus costos."), NOMEAN, NOMEAN },
    c_bep },
/* 1 */ { BI("Break-even Revenue", "Ingresos en el punto de equilibrio"),
    "BER", "FC x P", "P - VC", 0, 3,
    { { BI("Fixed costs", "Costos fijos"), "FC", 1000, 0 },
      { BI("Selling price per unit", "Precio de venta por unidad"), "P", 20, 0 },
      { BI("Variable cost per unit", "Costo variable por unidad"), "VC", 10, 0 } },
    MONEY,
    { BI("Sales of %s are needed so that revenue equals total costs.",
         "Se necesitan ventas de %s para que los ingresos igualen los costos totales."), NOMEAN, NOMEAN },
    c_ber },
/* 2 */ { BI("Total Revenue", "Ingresos totales"),
    "TR", "P x Q", NULL, 0, 2,
    { { BI("Selling price per unit", "Precio de venta por unidad"), "P", 20, 0 },
      { BI("Quantity sold", "Cantidad vendida"), "Q", 100, 0 } },
    MONEY,
    { BI("The business earns %s from selling these units.",
         "La empresa obtiene %s por vender estas unidades."), NOMEAN, NOMEAN },
    c_tr },
/* 3 */ { BI("Total Costs", "Costos totales"),
    "TC", "FC + VC x Q", NULL, 0, 3,
    { { BI("Fixed costs", "Costos fijos"), "FC", 1000, 0 },
      { BI("Variable cost per unit", "Costo variable por unidad"), "VC", 10, 0 },
      { BI("Quantity produced", "Cantidad producida"), "Q", 100, 0 } },
    MONEY,
    { BI("The business spends %s to produce this output.",
         "La empresa gasta %s para producir esta cantidad."), NOMEAN, NOMEAN },
    c_tc },
/* 4 */ { BI("Fixed Costs", "Costos fijos"),
    "FC", "TC - TVC", NULL, 0, 2,
    { { BI("Total costs", "Costos totales"), "TC", 5000, 0 },
      { BI("Total variable costs", "Costos variables totales"), "TVC", 3000, 0 } },
    MONEY,
    { BI("Fixed costs are %s. They do not change with output.",
         "Los costos fijos son %s. No cambian con la producción."), NOMEAN, NOMEAN },
    c_fc },
/* 5 */ { BI("Variable Costs", "Costos variables"),
    "TVC", "VC x Q", NULL, 0, 2,
    { { BI("Variable cost per unit", "Costo variable por unidad"), "VC", 10, 0 },
      { BI("Quantity produced", "Cantidad producida"), "Q", 300, 0 } },
    MONEY,
    { BI("Variable costs are %s. They rise when output rises.",
         "Los costos variables son %s. Aumentan cuando aumenta la producción."), NOMEAN, NOMEAN },
    c_tvc },
/* 6 */ { BI("Profit and Loss", "Beneficio y pérdida"),
    "Profit", "TR - TC", NULL, 0, 4,
    { { BI("Selling price per unit", "Precio de venta por unidad"), "P", 20, 0 },
      { BI("Quantity sold", "Cantidad vendida"), "Q", 150, 0 },
      { BI("Fixed costs", "Costos fijos"), "FC", 1000, 0 },
      { BI("Variable cost per unit", "Costo variable por unidad"), "VC", 10, 0 } },
    MONEY,
    { BI("Profit of %s: revenue is higher than total costs.",
         "Beneficio de %s: los ingresos son mayores que los costos totales."),
      BI("Loss of %s: total costs are higher than revenue.",
         "Pérdida de %s: los costos totales son mayores que los ingresos."),
      BI("Break-even: revenue equals total costs (no profit, no loss).",
         "Equilibrio: los ingresos igualan los costos totales (sin beneficio ni pérdida).") },
    c_profit },
/* 7 */ { BI("Contribution per Unit", "Contribución por unidad"),
    "c", "P - VC", NULL, 0, 2,
    { { BI("Selling price per unit", "Precio de venta por unidad"), "P", 20, 0 },
      { BI("Variable cost per unit", "Costo variable por unidad"), "VC", 10, 0 } },
    MONEY,
    { BI("Each unit sold gives %s to pay fixed costs and make profit.",
         "Cada unidad vendida aporta %s para pagar costos fijos y generar beneficio."),
      BI("Negative contribution: each unit loses %s before fixed costs.",
         "Contribución negativa: cada unidad pierde %s antes de los costos fijos."), NOMEAN },
    c_cpu },
/* 8 */ { BI("Contribution", "Contribución total"),
    "C", "(P - VC) x Q", NULL, 0, 3,
    { { BI("Selling price per unit", "Precio de venta por unidad"), "P", 20, 0 },
      { BI("Variable cost per unit", "Costo variable por unidad"), "VC", 10, 0 },
      { BI("Quantity sold", "Cantidad vendida"), "Q", 100, 0 } },
    MONEY,
    { BI("These sales contribute %s towards fixed costs and profit.",
         "Estas ventas aportan %s para cubrir costos fijos y beneficio."),
      BI("Negative total contribution of %s: sales do not even cover variable costs.",
         "Contribución total negativa de %s: las ventas no cubren ni los costos variables."), NOMEAN },
    c_contrib },
/* 9 */ { BI("Average Costs", "Costo medio"),
    "AC", "TC", "Q", 0, 2,
    { { BI("Total costs", "Costos totales"), "TC", 2000, 0 },
      { BI("Quantity produced", "Cantidad producida"), "Q", 100, 0 } },
    BI("currency units per unit", "unidades monetarias por unidad"),
    { BI("Each unit costs %s on average to produce.",
         "Cada unidad cuesta %s en promedio."), NOMEAN, NOMEAN },
    c_avgc },
/* 10 */ { BI("Average Revenue", "Ingreso medio"),
    "AR", "TR", "Q", 0, 2,
    { { BI("Total revenue", "Ingresos totales"), "TR", 2000, 0 },
      { BI("Quantity sold", "Cantidad vendida"), "Q", 100, 0 } },
    BI("currency units per unit", "unidades monetarias por unidad"),
    { BI("The business earns %s per unit sold on average.",
         "La empresa obtiene %s por unidad vendida en promedio."), NOMEAN, NOMEAN },
    c_avgr },
/* 11 */ { BI("Cash Flow", "Flujo de caja"),
    "CF", "Inflow - Outflow", NULL, 0, 2,
    { { BI("Cash inflow", "Entrada de efectivo"), "Inflow", 5000, 0 },
      { BI("Cash outflow", "Salida de efectivo"), "Outflow", 4000, 0 } },
    MONEY,
    { BI("More cash comes in than goes out: positive cash flow of %s.",
         "Entra más efectivo del que sale: flujo de caja positivo de %s."),
      BI("More cash goes out than comes in: negative cash flow of %s.",
         "Sale más efectivo del que entra: flujo de caja negativo de %s."),
      BI("Cash in equals cash out.", "La entrada de efectivo es igual a la salida.") },
    c_cf },
/* 12 */ { BI("Net Cash Flow", "Flujo de caja neto"),
    "NCF", "(Sales + Other in) - (Pay + Other out)", NULL, 0, 4,
    { { BI("Cash from sales", "Efectivo por ventas"), "Sales", 8000, 0 },
      { BI("Other cash inflows", "Otras entradas"), "Other in", 500, 0 },
      { BI("Cash payments (costs)", "Pagos de costos"), "Pay", 6500, 0 },
      { BI("Other cash outflows", "Otras salidas"), "Other out", 1000, 0 } },
    MONEY,
    { BI("Net cash flow is positive: the cash balance grows by %s.",
         "El flujo neto es positivo: el saldo de caja aumenta en %s."),
      BI("Net cash flow is negative: the cash balance falls by %s.",
         "El flujo neto es negativo: el saldo de caja disminuye en %s."),
      BI("Net cash flow is zero: the cash balance does not change.",
         "El flujo neto es cero: el saldo de caja no cambia.") },
    c_ncf },
/* 13 */ { BI("Closing Cash Balance", "Saldo final de caja"),
    "CCB", "Opening + (Inflows - Outflows)", NULL, 0, 3,
    { { BI("Opening cash balance", "Saldo inicial de caja"), "Opening", 2000, 0 },
      { BI("Cash inflows", "Entradas de efectivo"), "Inflows", 5000, 0 },
      { BI("Cash outflows", "Salidas de efectivo"), "Outflows", 4200, 0 } },
    MONEY,
    { BI("The business ends the period with %s in cash.",
         "La empresa termina el periodo con %s en caja."),
      BI("Negative balance of %s: the business needs extra funds (e.g. an overdraft).",
         "Saldo negativo de %s: la empresa necesita fondos extra (por ejemplo, un sobregiro)."), NOMEAN },
    c_ccb },
/* 14 */ { BI("Market Share", "Cuota de mercado"),
    "MS", "Company sales", "Total market sales", 1, 2,
    { { BI("Company sales", "Ventas de la empresa"), "Sales", 2500, 0 },
      { BI("Total market sales", "Ventas totales del mercado"), "Market", 10000, 0 } },
    PCT,
    { BI("The company has %s of the market.", "La empresa tiene el %s del mercado."), NOMEAN, NOMEAN },
    c_ms },
/* 15 */ { BI("Percentage Change", "Cambio porcentual"),
    "%chg", "New - Old", "Old", 1, 2,
    { { BI("Old value", "Valor anterior"), "Old", 100, 0 },
      { BI("New value", "Valor nuevo"), "New", 120, 0 } },
    PCT,
    { BI("The value increased by %s.", "El valor aumentó un %s."),
      BI("The value decreased by %s.", "El valor disminuyó un %s."),
      BI("There was no change.", "No hubo cambio.") },
    c_pc },
/* 16 */ { BI("Percentage Increase", "Aumento porcentual"),
    "%inc", "New - Old", "Old", 1, 2,
    { { BI("Old value", "Valor anterior"), "Old", 100, 0 },
      { BI("New value", "Valor nuevo"), "New", 125, 0 } },
    PCT,
    { BI("The value grew by %s compared with the old value.",
         "El valor creció un %s respecto al valor anterior."),
      BI("The result is negative, so the value actually fell by %s (use Percentage Decrease).",
         "El resultado es negativo: el valor en realidad bajó un %s (usa Disminución porcentual)."), NOMEAN },
    c_pi },
/* 17 */ { BI("Percentage Decrease", "Disminución porcentual"),
    "%dec", "Old - New", "Old", 1, 2,
    { { BI("Old value", "Valor anterior"), "Old", 100, 0 },
      { BI("New value", "Valor nuevo"), "New", 80, 0 } },
    PCT,
    { BI("The value fell by %s compared with the old value.",
         "El valor bajó un %s respecto al valor anterior."),
      BI("The result is negative, so the value actually rose by %s (use Percentage Increase).",
         "El resultado es negativo: el valor en realidad subió un %s (usa Aumento porcentual)."), NOMEAN },
    c_pd },
/* 18 */ { BI("Average", "Promedio"),
    "Avg", "Sum of values", "Number of values", 1, 6,
    { { BI("Value 1", "Valor 1"), "v1", 120, 0 },
      { BI("Value 2", "Valor 2"), "v2", 150, 0 },
      { BI("Value 3", "Valor 3"), "v3", 180, 0 },
      { BI("Value 4 (optional)", "Valor 4 (opcional)"), "v4", NAN, 1 },
      { BI("Value 5 (optional)", "Valor 5 (opcional)"), "v5", NAN, 1 },
      { BI("Value 6 (optional)", "Valor 6 (opcional)"), "v6", NAN, 1 } },
    NOUNIT,
    { BI("The typical (mean) value is %s.", "El valor típico (la media) es %s."), NOMEAN, NOMEAN },
    c_avg },
/* 19 */ { BI("Total Sales", "Ventas totales"),
    "TS", "S1 + S2 + ...", NULL, 1, 6,
    { { BI("Sales 1", "Ventas 1"), "S1", 1200, 0 },
      { BI("Sales 2", "Ventas 2"), "S2", 1500, 0 },
      { BI("Sales 3", "Ventas 3"), "S3", 1800, 0 },
      { BI("Sales 4 (optional)", "Ventas 4 (opcional)"), "S4", NAN, 1 },
      { BI("Sales 5 (optional)", "Ventas 5 (opcional)"), "S5", NAN, 1 },
      { BI("Sales 6 (optional)", "Ventas 6 (opcional)"), "S6", NAN, 1 } },
    MONEY,
    { BI("Total sales for all the periods or products: %s.",
         "Ventas totales de todos los periodos o productos: %s."), NOMEAN, NOMEAN },
    c_ts },
};

const int FORMULAS_N = (int)(sizeof FORMULAS / sizeof FORMULAS[0]);
