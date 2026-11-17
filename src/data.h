#ifndef DATA_H
#define DATA_H

#include <stddef.h>

/* ---- bilingual string: .en / .es ------------------------------------ */
typedef struct { const char *en, *es; } bi_t;
#define BI(e, s) { e, s }

extern int g_lang; /* 0 = English, 1 = Espanol */
static inline const char *tr(bi_t b) { return g_lang ? b.es : b.en; }

/* ---- formulas -------------------------------------------------------- */
#define MAXV 6
#define MAXSTEPS 6

typedef struct {
    bi_t name;
    const char *sym;   /* language-independent symbol, e.g. "FC" */
    double def;        /* default value */
    int optional;      /* 1 = may be left blank (NAN) */
} var_t;

/* One line of the procedure: pre [fraction num/den | plain text] post */
typedef struct {
    char pre[20], num[72], den[72], post[20];
} step_t;

typedef struct {
    double result;
    int err;    /* 0 ok, 1 division by zero, 2 price <= variable cost, 3 no values */
    int msel;   /* which meaning sentence to show (0..2) */
    int nsteps;
    step_t st[MAXSTEPS];
} result_t;

typedef struct {
    bi_t name;
    const char *lhs, *num, *den; /* displayed expression: lhs = num / den */
    int group;                   /* 0 accounting, 1 market */
    int nvars;
    var_t vars[MAXV];
    bi_t unit;
    bi_t meaning[3];             /* printf formats with one %s */
    void (*calc)(const double *v, result_t *r);
} formula_t;

extern const formula_t FORMULAS[];
extern const int FORMULAS_N;
extern const bi_t ERRMSG[4];

/* ---- reference content ----------------------------------------------- */
#define NCATS 7
extern const bi_t CATS[NCATS];

typedef struct { bi_t term, def, expl, ex; int cat; } dict_t;
typedef struct { bi_t title, body; int cat; } info_t;
typedef struct { bi_t name, def, purpose, includes, ex; } plan_t;
typedef struct { bi_t term, meaning, question, answer; } cmd_t;

extern const dict_t DICT[];  extern const int DICT_N;
extern const info_t INFO[];  extern const int INFO_N;
extern const plan_t PLAN[];  extern const int PLAN_N;
extern const cmd_t  CMD[];   extern const int CMD_N;

/* ---- charts ------------------------------------------------------------ */
int    chart_count(void);
bi_t   chart_name(int c);
int    chart_nparams(int c);
void   chart_param_label(int c, int p, char *buf, size_t n);
double *chart_values(int c);
void   chart_draw(int c);

#endif
