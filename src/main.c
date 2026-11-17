#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "data.h"

/* ---------------------------------------------------------------- *
 * Small helpers                                                     *
 * ---------------------------------------------------------------- */
#define BIL(e, s) ((bi_t){ e, s })
#define NOFT { NULL, NULL }

enum { T_FORM, T_DICT, T_INFO, T_PLAN, T_CMD };
static const char TAGS[5] = { 'F', 'D', 'I', 'P', 'C' };

typedef struct { int type, idx; } ref_t;

#define MAXFAV 40
#define MAXHIST 25
static ref_t favs[MAXFAV];
static int nfav = 0;

typedef struct { int f; double in[MAXV]; double res; } hist_t;
static hist_t hist[MAXHIST];
static int nhist = 0;

static double fvals[32][MAXV]; /* current inputs of every formula */

static void foot(const bi_t *f)
{
    const char *l[6];
    for (int i = 0; i < 5; i++) l[i] = (f && f[i].en) ? tr(f[i]) : NULL;
    l[5] = g_lang ? "IDIOMA ES" : "LANG EN";
    ui_footer(l);
}

static int fkey_index(int k)
{
    switch (k) {
    case KEY_F1: return 0; case KEY_F2: return 1; case KEY_F3: return 2;
    case KEY_F4: return 3; case KEY_F5: return 4;
    default: return -1;
    }
}

/* ---------------------------------------------------------------- *
 * Favorites and history                                             *
 * ---------------------------------------------------------------- */
static int fav_find(int t, int i)
{
    for (int k = 0; k < nfav; k++)
        if (favs[k].type == t && favs[k].idx == i) return k;
    return -1;
}

static void fav_toggle(int t, int i)
{
    int k = fav_find(t, i);
    if (k >= 0) {
        memmove(&favs[k], &favs[k + 1], (size_t)(nfav - k - 1) * sizeof(ref_t));
        nfav--;
    } else if (nfav < MAXFAV) {
        favs[nfav].type = t;
        favs[nfav].idx = i;
        nfav++;
    } else {
        ui_msg(g_lang ? "La lista de favoritos está llena." : "The favorites list is full.");
    }
}

static void hist_push(int f, const double *in, double res)
{
    memmove(&hist[1], &hist[0], (MAXHIST - 1) * sizeof(hist_t));
    hist[0].f = f;
    for (int i = 0; i < MAXV; i++) hist[0].in[i] = in[i];
    hist[0].res = res;
    if (nhist < MAXHIST) nhist++;
}

/* ---------------------------------------------------------------- *
 * Entries (formula / dictionary / info / plan / command term)       *
 * ---------------------------------------------------------------- */
static bi_t entry_name(int t, int i)
{
    switch (t) {
    case T_FORM: return FORMULAS[i].name;
    case T_DICT: return DICT[i].term;
    case T_INFO: return INFO[i].title;
    case T_PLAN: return PLAN[i].name;
    default:     return CMD[i].term;
    }
}

static int entry_count(int t)
{
    switch (t) {
    case T_FORM: return FORMULAS_N;
    case T_DICT: return DICT_N;
    case T_INFO: return INFO_N;
    case T_PLAN: return PLAN_N;
    default:     return CMD_N;
    }
}

static void build_text(int t, int i, char *b, size_t n)
{
    switch (t) {
    case T_DICT:
        snprintf(b, n, "%s:\n%s\n\n%s:\n%s\n\n%s:\n%s",
                 g_lang ? "Definición" : "Definition", tr(DICT[i].def),
                 g_lang ? "Explicación" : "Explanation", tr(DICT[i].expl),
                 g_lang ? "Ejemplo" : "Example", tr(DICT[i].ex));
        break;
    case T_INFO:
        snprintf(b, n, "%s", tr(INFO[i].body));
        break;
    case T_PLAN:
        snprintf(b, n, "%s:\n%s\n\n%s:\n%s\n\n%s:\n%s\n\n%s:\n%s",
                 g_lang ? "Definición" : "Definition", tr(PLAN[i].def),
                 g_lang ? "Propósito" : "Main purpose", tr(PLAN[i].purpose),
                 g_lang ? "Incluye" : "It includes", tr(PLAN[i].includes),
                 g_lang ? "Ejemplo" : "Example", tr(PLAN[i].ex));
        break;
    default:
        snprintf(b, n, "%s:\n%s\n\n%s:\n%s\n\n%s:\n%s",
                 g_lang ? "Qué significa" : "What it requires", tr(CMD[i].meaning),
                 g_lang ? "Pregunta de ejemplo" : "Example question", tr(CMD[i].question),
                 g_lang ? "Respuesta adecuada" : "A good answer", tr(CMD[i].answer));
        break;
    }
}

static void view_entry(int t, int i)
{
    static char txt[1500];
    const char *lines[90];
    int scroll = 0;
    for (;;) {
        build_text(t, i, txt, sizeof txt);
        int n = ui_wrap(txt, SCR_W - 24, lines, 90);
        int vis = (FOOT_Y - HEAD_H - 6) / g_lh;
        int maxs = n > vis ? n - vis : 0;
        if (scroll > maxs) scroll = maxs;
        dclear(COL_BG);
        for (int r = 0; r < vis && scroll + r < n; r++) {
            const char *ln = lines[scroll + r];
            size_t L = strlen(ln);
            ui_text(8, HEAD_H + 4 + r * g_lh, (L && ln[L - 1] == ':') ? COL_ACC : COL_TXT, ln);
        }
        if (maxs > 0) {
            int h = FOOT_Y - HEAD_H - 4, bh = h * vis / n;
            drect(SCR_W - 6, HEAD_H + 2 + (h - bh) * scroll / maxs, SCR_W - 3,
                  HEAD_H + 2 + (h - bh) * scroll / maxs + bh, COL_ACC);
        }
        ui_header(tr(entry_name(t, i)));
        if (fav_find(t, i) >= 0) ui_text(SCR_W - 30, (HEAD_H - g_th) / 2, C_WHITE, "*");
        foot((bi_t[5]){ BI("Fav", "Fav"), NOFT, NOFT, NOFT, NOFT });
        dupdate();
        int k = getkey().key;
        if (k == KEY_EXIT) return;
        if (k == KEY_UP && scroll > 0) scroll--;
        else if (k == KEY_DOWN && scroll < maxs) scroll++;
        else if (k == KEY_F1) fav_toggle(t, i);
        else if (k == KEY_F6) g_lang ^= 1;
    }
}

/* ---------------------------------------------------------------- *
 * Generic scrolling list                                            *
 * ---------------------------------------------------------------- */
typedef const char *(*lbl_fn)(int i, char *buf, size_t n, const void *ctx);

/* returns index (EXE), -1 (EXIT) or -2 (F1..F5 pressed, *fk = 1..5) */
static int pick_list(bi_t title, int n, lbl_fn lbl, const void *ctx, int *sel,
                     const bi_t *ft, int *fk)
{
    int top = 0, rh = g_lh + 2, y0 = HEAD_H + 3;
    int rows = (FOOT_Y - y0) / rh;
    if (fk) *fk = 0;
    for (;;) {
        if (*sel >= n) *sel = n - 1;
        if (*sel < 0) *sel = 0;
        if (*sel < top) top = *sel;
        if (*sel >= top + rows) top = *sel - rows + 1;
        dclear(COL_BG);
        for (int r = 0; r < rows && top + r < n; r++) {
            int i = top + r, y = y0 + r * rh;
            char buf[110];
            const char *s = lbl(i, buf, sizeof buf, ctx);
            if (i == *sel) drect(0, y - 1, SCR_W - 9, y + rh - 2, COL_SEL);
            ui_text(8, y, i == *sel ? COL_ACC : COL_TXT, s);
        }
        if (n > rows) {
            int h = FOOT_Y - y0, bh = h * rows / n;
            drect(SCR_W - 6, y0 + (h - bh) * top / (n - rows), SCR_W - 3,
                  y0 + (h - bh) * top / (n - rows) + bh, COL_ACC);
        }
        ui_header(tr(title));
        foot(ft);
        dupdate();
        int k = getkey().key;
        int fi = fkey_index(k);
        if (fi >= 0) {
            if (ft && ft[fi].en) { if (fk) *fk = fi + 1; return -2; }
            continue;
        }
        switch (k) {
        case KEY_UP:    if (n > 0) *sel = *sel > 0 ? *sel - 1 : n - 1; break;
        case KEY_DOWN:  if (n > 0) *sel = *sel < n - 1 ? *sel + 1 : 0; break;
        case KEY_LEFT:  *sel -= rows; if (*sel < 0) *sel = 0; break;
        case KEY_RIGHT: *sel += rows; if (*sel > n - 1) *sel = n - 1; break;
        case KEY_EXE:   if (n > 0) return *sel; break;
        case KEY_EXIT:  return -1;
        case KEY_F6:    g_lang ^= 1; break;
        }
    }
}

static const char *lbl_bi(int i, char *buf, size_t n, const void *ctx)
{
    (void)buf; (void)n;
    return tr(((const bi_t *)ctx)[i]);
}

static const char *lbl_ref(int i, char *buf, size_t n, const void *ctx)
{
    const ref_t *r = (const ref_t *)ctx;
    snprintf(buf, n, "[%c] %s%s", TAGS[r[i].type],
             fav_find(r[i].type, r[i].idx) >= 0 ? "* " : "", tr(entry_name(r[i].type, r[i].idx)));
    return buf;
}

static const char *lbl_idx(int i, char *buf, size_t n, const void *ctx)
{   /* ctx: int array of entry indices, first element = entry type */
    const int *a = (const int *)ctx;
    int t = a[0], e = a[i + 1];
    snprintf(buf, n, "%s%s", fav_find(t, e) >= 0 ? "* " : "", tr(entry_name(t, e)));
    return buf;
}

/* ---------------------------------------------------------------- *
 * Formula screens                                                   *
 * ---------------------------------------------------------------- */
/* lhs = num / den, drawn with a real fraction bar. Returns the height. */
static int draw_expr(const char *lhs, const char *num, const char *den, int x, int y, int draw)
{
    char head[40];
    snprintf(head, sizeof head, "%s =", lhs);
    if (!den) {
        char t[110];
        snprintf(t, sizeof t, "%s %s", head, num);
        if (draw) ui_text(x, y, COL_ACC, t);
        return g_th + 4;
    }
    int hw = ui_w(head) + 6, nw = ui_w(num), dw = ui_w(den);
    int fw = (nw > dw ? nw : dw) + 6, h = 2 * g_th + 6, mid = y + g_th + 2;
    if (draw) {
        ui_text(x, mid - g_th / 2, COL_ACC, head);
        ui_text(x + hw + (fw - nw) / 2, y, COL_ACC, num);
        dline(x + hw, mid, x + hw + fw, mid, COL_ACC);
        ui_text(x + hw + (fw - dw) / 2, mid + 3, COL_ACC, den);
    }
    return h + 2;
}

static int draw_step(const step_t *s, int x, int y, int draw)
{
    if (!s->den[0]) {
        char t[110];
        snprintf(t, sizeof t, "%s %s", s->pre, s->num);
        if (draw) ui_text(x, y, COL_TXT, t);
        return g_th + 4;
    }
    int pw = ui_w(s->pre) + 6, nw = ui_w(s->num), dw = ui_w(s->den);
    int fw = (nw > dw ? nw : dw) + 6, h = 2 * g_th + 6, mid = y + g_th + 2;
    if (draw) {
        ui_text(x, mid - g_th / 2, COL_TXT, s->pre);
        ui_text(x + pw + (fw - nw) / 2, y, COL_TXT, s->num);
        dline(x + pw, mid, x + pw + fw, mid, COL_TXT);
        ui_text(x + pw + (fw - dw) / 2, mid + 3, COL_TXT, s->den);
        if (s->post[0]) ui_text(x + pw + fw + 6, mid - g_th / 2, COL_TXT, s->post);
    }
    return h + 2;
}

static int render_result(int fi, const result_t *r, int y0, int draw)
{
    const formula_t *f = &FORMULAS[fi];
    char num[48], ans[120], mean[260];
    int y = y0, x = 8;
    y += draw_expr(f->lhs, f->num, f->den, x, y, draw) + 4;
    if (draw) ui_text(x, y, COL_MUTE, g_lang ? "Procedimiento:" : "Procedure:");
    y += g_lh;
    for (int i = 0; i < r->nsteps; i++) y += draw_step(&r->st[i], x + 6, y, draw);
    y += 4;
    fmt_num(r->result, num, sizeof num, 2);
    {
        const char *u = tr(f->unit);
        if (u[0] == '%' || u[0] == 0) snprintf(ans, sizeof ans, "%s%s", num, u);
        else snprintf(ans, sizeof ans, "%s %s", num, u);
    }
    if (draw) {
        drect(x - 4, y - 2, SCR_W - 10, y + g_lh + 3, COL_SEL);
        ui_text(x, y + 1, COL_ACC, g_lang ? "Respuesta:" : "Answer:");
        int ax = x + ui_w(g_lang ? "Respuesta:" : "Answer:") + 8;
        ui_text(ax, y + 1, COL_ACC, ans);
        ui_text(ax + 1, y + 1, COL_ACC, ans);
    }
    y += g_lh + 8;
    {
        char absn[48];
        fmt_num(fabs(r->result), absn, sizeof absn, 2);
        snprintf(mean, sizeof mean, tr(f->meaning[r->msel]), absn);
    }
    const char *lines[8];
    int n = ui_wrap(mean, SCR_W - 30, lines, 8);
    if (draw) ui_text(x, y, COL_MUTE, g_lang ? "Significado:" : "Meaning:");
    y += g_lh;
    for (int i = 0; i < n; i++) {
        if (draw) ui_text(x + 6, y, COL_TXT, lines[i]);
        y += g_lh;
    }
    return y - y0 + 6;
}

static void screen_result(int fi, const result_t *r)
{
    int scroll = 0, vis = FOOT_Y - HEAD_H - 4;
    for (;;) {
        int total = render_result(fi, r, 0, 0);
        int maxs = total > vis ? total - vis : 0;
        if (scroll > maxs) scroll = maxs;
        dclear(COL_BG);
        render_result(fi, r, HEAD_H + 4 - scroll, 1);
        ui_header(tr(FORMULAS[fi].name));
        if (fav_find(T_FORM, fi) >= 0) ui_text(SCR_W - 30, (HEAD_H - g_th) / 2, C_WHITE, "*");
        foot((bi_t[5]){ BI("Edit", "Editar"), BI("Fav", "Fav"), BI("To Ans", "A Ans"), NOFT, NOFT });
        dupdate();
        int k = getkey().key;
        if (k == KEY_EXIT || k == KEY_EXE || k == KEY_F1) return;
        if (k == KEY_UP) { scroll -= g_lh; if (scroll < 0) scroll = 0; }
        else if (k == KEY_DOWN) { scroll += g_lh; if (scroll > maxs) scroll = maxs; }
        else if (k == KEY_F2) fav_toggle(T_FORM, fi);
        else if (k == KEY_F3) {
            calc_set_ans(r->result);
            ui_msg(g_lang ? "Resultado guardado en Ans de la calculadora." : "Result stored in Ans of the calculator.");
        }
        else if (k == KEY_F6) g_lang ^= 1;
    }
}

static void screen_formula(int fi)
{
    const formula_t *f = &FORMULAS[fi];
    double *vals = fvals[fi];
    int sel = 0;
    for (;;) {
        dclear(COL_BG);
        int y = HEAD_H + 4;
        y += draw_expr(f->lhs, f->num, f->den, 8, y, 1) + 8;
        int rh = g_lh + 4;
        for (int i = 0; i < f->nvars; i++) {
            int yy = y + i * rh;
            char b[48];
            if (i == sel) drect(2, yy - 2, SCR_W - 3, yy + rh - 3, COL_SEL);
            ui_text(8, yy, COL_ACC, f->vars[i].sym);
            ui_text(70, yy, COL_TXT, tr(f->vars[i].name));
            if (vals[i] != vals[i]) snprintf(b, sizeof b, "-"); else fmt_num(vals[i], b, sizeof b, 4);
            ui_text_r(SCR_W - 10, yy, COL_TXT, b);
        }
        ui_header(tr(f->name));
        if (fav_find(T_FORM, fi) >= 0) ui_text(SCR_W - 30, (HEAD_H - g_th) / 2, C_WHITE, "*");
        foot((bi_t[5]){ BI("Calculate", "Calcular"), BI("Fav", "Fav"), BI("Reset", "Reiniciar"), NOFT, NOFT });
        ui_text(8, FOOT_Y - g_lh - 1, COL_MUTE,
                g_lang ? "EXE = editar valor   DEL = borrar" : "EXE = edit value   DEL = clear");
        dupdate();
        int k = getkey().key;
        if (k == KEY_EXIT) return;
        if (k == KEY_UP) sel = sel > 0 ? sel - 1 : f->nvars - 1;
        else if (k == KEY_DOWN) sel = sel < f->nvars - 1 ? sel + 1 : 0;
        else if (k == KEY_EXE) {
            char lb[80];
            snprintf(lb, sizeof lb, "%s (%s)", tr(f->vars[sel].name), f->vars[sel].sym);
            ui_edit_number(lb, &vals[sel]);
        }
        else if (k == KEY_DEL) vals[sel] = f->vars[sel].optional ? (double)NAN : 0.0;
        else if (k == KEY_F2) fav_toggle(T_FORM, fi);
        else if (k == KEY_F3) { for (int i = 0; i < f->nvars; i++) vals[i] = f->vars[i].def; }
        else if (k == KEY_F6) g_lang ^= 1;
        else if (k == KEY_F1) {
            static result_t r;
            memset(&r, 0, sizeof r);
            f->calc(vals, &r);
            if (r.err) { ui_msg(tr(ERRMSG[r.err])); continue; }
            hist_push(fi, vals, r.result);
            screen_result(fi, &r);
        }
    }
}

static void screen_formula_list(void)
{
    static int sel = 0;
    int first_market = 0;
    for (int i = 0; i < FORMULAS_N; i++)
        if (FORMULAS[i].group == 1) { first_market = i; break; }
    for (;;) {
        int idx[33], fk;
        idx[0] = T_FORM;
        for (int i = 0; i < FORMULAS_N; i++) idx[i + 1] = i;
        int r = pick_list(BIL("Formulas", "Fórmulas"), FORMULAS_N, lbl_idx, idx, &sel,
                          (bi_t[5]){ BI("Accounting", "Contab."), BI("Market", "Mercado"), NOFT, NOFT, NOFT }, &fk);
        if (r == -1) return;
        if (r == -2) { sel = fk == 1 ? 0 : first_market; continue; }
        screen_formula(r);
    }
}

/* ---------------------------------------------------------------- *
 * Dictionary / information / plan / command terms                   *
 * ---------------------------------------------------------------- */
static const char *lbl_cats(int i, char *buf, size_t n, const void *ctx)
{
    (void)buf; (void)n; (void)ctx;
    if (i < NCATS) return tr(CATS[i]);
    return g_lang ? "Todos los términos" : "All terms";
}

static void category_screen(int type, bi_t title)
{
    static int csel[2] = { 0, 0 };
    int slot = type == T_DICT ? 0 : 1;
    for (;;) {
        int r = pick_list(title, NCATS + (type == T_DICT ? 1 : 0), lbl_cats, NULL, &csel[slot], NULL, NULL);
        if (r < 0) return;
        int idx[130], n = 0, sel = 0;
        idx[0] = type;
        int total = entry_count(type);
        for (int i = 0; i < total; i++) {
            int cat = type == T_DICT ? DICT[i].cat : INFO[i].cat;
            if (r == NCATS || cat == r) idx[1 + n++] = i;
        }
        for (;;) {
            int e = pick_list(r < NCATS ? CATS[r] : BIL("All terms", "Todos los términos"),
                              n, lbl_idx, idx, &sel, NULL, NULL);
            if (e < 0) break;
            view_entry(type, idx[1 + e]);
        }
    }
}

static const char *lbl_plan(int i, char *buf, size_t n, const void *ctx)
{
    int t = *(const int *)ctx;
    snprintf(buf, n, "%s%s", fav_find(t, i) >= 0 ? "* " : "", tr(entry_name(t, i)));
    return buf;
}

static void simple_list_screen(int type, bi_t title)
{
    int sel = 0;
    for (;;) {
        int r = pick_list(title, entry_count(type), lbl_plan, &type, &sel, NULL, NULL);
        if (r < 0) return;
        view_entry(type, r);
    }
}

/* ---------------------------------------------------------------- *
 * Charts                                                            *
 * ---------------------------------------------------------------- */
static const char *lbl_chart(int i, char *buf, size_t n, const void *ctx)
{
    (void)buf; (void)n; (void)ctx;
    return tr(chart_name(i));
}

static const char *lbl_param(int i, char *buf, size_t n, const void *ctx)
{
    int c = *(const int *)ctx;
    char name[60], v[40];
    chart_param_label(c, i, name, sizeof name);
    fmt_num(chart_values(c)[i], v, sizeof v, 4);
    snprintf(buf, n, "%s: %s", name, v);
    return buf;
}

static void chart_edit(int c)
{
    int sel = 0;
    for (;;) {
        int r = pick_list(BIL("Edit values", "Editar valores"), chart_nparams(c), lbl_param, &c, &sel, NULL, NULL);
        if (r < 0) return;
        char name[60];
        chart_param_label(c, r, name, sizeof name);
        ui_edit_number(name, &chart_values(c)[r]);
    }
}

static void chart_screen(int c)
{
    for (;;) {
        dclear(COL_BG);
        chart_draw(c);
        ui_header(tr(chart_name(c)));
        foot((bi_t[5]){ BI("Edit", "Editar"), BI("Prev", "Ant."), BI("Next", "Sig."), NOFT, NOFT });
        dupdate();
        int k = getkey().key;
        if (k == KEY_EXIT) return;
        if (k == KEY_F1 || k == KEY_EXE) chart_edit(c);
        else if (k == KEY_F2 || k == KEY_LEFT) c = (c + chart_count() - 1) % chart_count();
        else if (k == KEY_F3 || k == KEY_RIGHT) c = (c + 1) % chart_count();
        else if (k == KEY_F6) g_lang ^= 1;
    }
}

static void charts_menu(void)
{
    int sel = 0;
    for (;;) {
        int r = pick_list(BIL("Business graphs", "Gráficos empresariales"), chart_count(), lbl_chart, NULL, &sel, NULL, NULL);
        if (r < 0) return;
        chart_screen(r);
    }
}

/* ---------------------------------------------------------------- *
 * Scientific calculator                                             *
 * ---------------------------------------------------------------- */
static void backspace(char *e)
{
    size_t n = strlen(e);
    if (!n) return;
    if (e[n - 1] == '(') {
        e[--n] = 0;
        while (n && e[n - 1] >= 'a' && e[n - 1] <= 'z') e[--n] = 0;
    } else if (e[n - 1] >= 'a' && e[n - 1] <= 'z') {
        while (n && e[n - 1] >= 'a' && e[n - 1] <= 'z') e[--n] = 0;
    } else {
        e[n - 1] = 0;
    }
}

static void append(char *e, size_t cap, const char *s)
{
    if (strlen(e) + strlen(s) < cap) strcat(e, s);
}

static void screen_calc(void)
{
    char expr[100] = "", last[100] = "", hl[4][100], hr[4][48];
    int nh = 0, err = 0;
    for (;;) {
        dclear(COL_BG);
        int y = HEAD_H + 4;
        for (int i = 0; i < nh; i++) {
            char d[100];
            size_t j = 0;
            for (; hl[i][j] && j < sizeof d - 1; j++) d[j] = hl[i][j] == '*' ? 'x' : hl[i][j];
            d[j] = 0;
            ui_text(6, y, COL_MUTE, d);
            ui_text_r(SCR_W - 8, y + g_lh, COL_ACC, hr[i]);
            y += 2 * g_lh + 2;
        }
        int ey = FOOT_Y - 3 * g_lh;
        dline(0, ey - 3, SCR_W - 1, ey - 3, COL_GRID);
        {
            char d[110];
            size_t j = 0;
            for (; expr[j] && j < sizeof d - 3; j++) d[j] = expr[j] == '*' ? 'x' : expr[j];
            d[j++] = '_';
            d[j] = 0;
            ui_text(6, ey, COL_TXT, d);
        }
        if (err) ui_text_r(SCR_W - 8, ey + g_lh, COL_BAD, g_lang ? "Error" : "Error");
        ui_text(6, ey + g_lh + 2, COL_MUTE, g_deg ? "DEG" : "RAD");
        ui_header(g_lang ? "Calculadora científica" : "Scientific calculator");
        foot((bi_t[5]){ BI("sqrt", "sqrt"), BI("pi", "pi"), BI("Ans", "Ans"), BI("abs", "abs"), BI("DEG/RAD", "DEG/RAD") });
        dupdate();
        key_event_t ev = getkey();
        int k = ev.key;
        if (k == KEY_EXIT) {
            if (expr[0]) { expr[0] = 0; err = 0; } else return;
        } else if (k == KEY_EXE) {
            double v;
            if (!expr[0]) continue;
            if (calc_eval(expr, &v) == 0) {
                err = 0;
                if (nh == 4) { memmove(hl[0], hl[1], 3 * sizeof hl[0]); memmove(hr[0], hr[1], 3 * sizeof hr[0]); nh = 3; }
                snprintf(hl[nh], sizeof hl[nh], "%s", expr);
                fmt_g(v, hr[nh], sizeof hr[nh]);
                nh++;
                snprintf(last, sizeof last, "%s", expr);
                calc_set_ans(v);
                expr[0] = 0;
            } else err = 1;
        } else if (k == KEY_DEL) { backspace(expr); err = 0; }
        else if (k == KEY_UP) { snprintf(expr, sizeof expr, "%s", last); err = 0; }
        else if (k == KEY_F1) append(expr, sizeof expr, "sqrt(");
        else if (k == KEY_F2) append(expr, sizeof expr, "pi");
        else if (k == KEY_F3) append(expr, sizeof expr, "ans");
        else if (k == KEY_F4) append(expr, sizeof expr, "abs(");
        else if (k == KEY_F5) g_deg ^= 1;
        else if (k == KEY_F6) g_lang ^= 1;
        else if (k == KEY_SIN) append(expr, sizeof expr, ev.shift ? "asin(" : "sin(");
        else if (k == KEY_COS) append(expr, sizeof expr, ev.shift ? "acos(" : "cos(");
        else if (k == KEY_TAN) append(expr, sizeof expr, ev.shift ? "atan(" : "tan(");
        else if (k == KEY_LN) append(expr, sizeof expr, ev.shift ? "exp(" : "ln(");
        else if (k == KEY_LOG) append(expr, sizeof expr, "log(");
        else if (k == KEY_SQUARE) append(expr, sizeof expr, "^2");
        else {
            int c = ui_key_char(k);
            if (c) { char s[2] = { (char)c, 0 }; append(expr, sizeof expr, s); err = 0; }
        }
    }
}

/* ---------------------------------------------------------------- *
 * Favorites, history, search                                        *
 * ---------------------------------------------------------------- */
static void open_ref(int t, int i)
{
    if (t == T_FORM) screen_formula(i); else view_entry(t, i);
}

static void screen_favs(void)
{
    int sel = 0;
    for (;;) {
        int fk;
        int r = pick_list(BIL("Favorites", "Favoritos"), nfav, lbl_ref, favs, &sel,
                          (bi_t[5]){ BI("Remove", "Quitar"), NOFT, NOFT, NOFT, NOFT }, &fk);
        if (r == -1) return;
        if (r == -2) { if (nfav) fav_toggle(favs[sel].type, favs[sel].idx); continue; }
        open_ref(favs[r].type, favs[r].idx);
    }
}

static const char *lbl_hist(int i, char *buf, size_t n, const void *ctx)
{
    (void)ctx;
    char num[48];
    const char *u = tr(FORMULAS[hist[i].f].unit);
    fmt_num(hist[i].res, num, sizeof num, 2);
    snprintf(buf, n, "%s = %s%s%s", tr(FORMULAS[hist[i].f].name), num,
             (u[0] && u[0] != '%') ? " " : "", u);
    return buf;
}

static void screen_hist(void)
{
    int sel = 0;
    for (;;) {
        int fk;
        int r = pick_list(BIL("Calculation history", "Historial de cálculos"), nhist, lbl_hist, NULL, &sel,
                          (bi_t[5]){ BI("Clear", "Borrar"), NOFT, NOFT, NOFT, NOFT }, &fk);
        if (r == -1) return;
        if (r == -2) { nhist = 0; sel = 0; continue; }
        for (int i = 0; i < MAXV; i++) fvals[hist[r].f][i] = hist[r].in[i];
        screen_formula(hist[r].f);
    }
}

static int do_search(const char *q, ref_t *out, int max)
{
    char nq[40], a[160], b[160];
    int n = 0;
    ui_fold(q, nq, sizeof nq);
    for (int t = 0; t < 5; t++)
        for (int i = 0; i < entry_count(t) && n < max; i++) {
            bi_t nm = entry_name(t, i);
            ui_fold(nm.en, a, sizeof a);
            ui_fold(nm.es, b, sizeof b);
            if (strstr(a, nq) || strstr(b, nq)) { out[n].type = t; out[n].idx = i; n++; }
        }
    return n;
}

static void screen_search(void)
{
    static char q[32] = "";
    static ref_t res[60];
    for (;;) {
        if (!ui_edit_text(g_lang ? "Buscar" : "Search", q, sizeof q)) return;
        int n = do_search(q, res, 60), sel = 0;
        if (!n) {
            ui_msg(g_lang ? "Sin resultados." : "No results.");
            continue;
        }
        for (;;) {
            int r = pick_list(BIL("Results", "Resultados"), n, lbl_ref, res, &sel, NULL, NULL);
            if (r < 0) break;
            open_ref(res[r].type, res[r].idx);
        }
    }
}

/* ---------------------------------------------------------------- *
 * Main menu                                                         *
 * ---------------------------------------------------------------- */
static const bi_t MENU[] = {
    BI("1. Formulas", "1. Fórmulas"),
    BI("2. Business graphs", "2. Gráficos empresariales"),
    BI("3. Dictionary", "3. Diccionario"),
    BI("4. Business information", "4. Información empresarial"),
    BI("5. Business plan guide", "5. Guía del plan de negocio"),
    BI("6. Command terms", "6. Términos de instrucción"),
    BI("7. Scientific calculator", "7. Calculadora científica"),
    BI("8. Favorites", "8. Favoritos"),
    BI("9. Calculation history", "9. Historial de cálculos"),
    BI("0. Search", "0. Buscar"),
};
#define MENU_N ((int)(sizeof MENU / sizeof MENU[0]))

static void run_menu(int i)
{
    switch (i) {
    case 0: screen_formula_list(); break;
    case 1: charts_menu(); break;
    case 2: category_screen(T_DICT, BIL("Dictionary", "Diccionario")); break;
    case 3: category_screen(T_INFO, BIL("Business information", "Información empresarial")); break;
    case 4: simple_list_screen(T_PLAN, BIL("Business plan guide", "Guía del plan de negocio")); break;
    case 5: simple_list_screen(T_CMD, BIL("Command terms", "Términos de instrucción")); break;
    case 6: screen_calc(); break;
    case 7: screen_favs(); break;
    case 8: screen_hist(); break;
    default: screen_search(); break;
    }
}

int main(void)
{
    int sel = 0, fk;
    ui_init();
    for (int f = 0; f < FORMULAS_N; f++)
        for (int i = 0; i < FORMULAS[f].nvars; i++) fvals[f][i] = FORMULAS[f].vars[i].def;
    for (;;) {
        int r = pick_list(BIL("BUSINESS FX", "BUSINESS FX"), MENU_N, lbl_bi, MENU, &sel,
                          (bi_t[5]){ BI("Search", "Buscar"), NOFT, NOFT, NOFT, NOFT }, &fk);
        if (r == -1) break;
        if (r == -2) { screen_search(); continue; }
        run_menu(r);
    }
    return 1;
}
