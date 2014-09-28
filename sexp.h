#ifndef SEXP_H
#define SEXP_H

enum {
  ty_float, ty_integer, ty_symbol, ty_sxp
};

typedef struct t_sxp {
  int type;
  union {
    int Z;
    double R;
    char *symbol;
    struct t_sxp *down;
  };
  struct t_sxp *next;
} sxp;

sxp *sxp_makeint(int Z, sxp *n);
sxp *sxp_makefloat(double R, sxp *n);
sxp *sxp_makesymbol(char *sym, sxp *n);
sxp *sxp_makesxp(sxp *d, sxp *n);
void sxp_dest(sxp *x);
void sxp_print(sxp *x);

void set_reader(int (*read)(void));
sxp *sxp_next();

int sxp_length(sxp *s);
int sxp_isequal(sxp *a, sxp *b);


//void sxp_assert_type(sxp *x, int type);
#define sxp_assert_type(s, t) assert((s)->type == (t))

#endif
