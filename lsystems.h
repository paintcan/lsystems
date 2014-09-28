#ifndef LSYSTEMS_H
#define LSYSTEMS_H

#include "sexp.h"
#include <vector>
#include <map>
#include <string>

typedef struct t_stochastic_expansion {
  float probability;
  sxp *expansion;
} stochastic_expansion;

typedef struct t_production {
  std::vector<sxp *> left;
  sxp *center;
  std::vector<sxp *> right;
  sxp *condition;
  std::vector<stochastic_expansion *> expansion;
} production;

production *parse_production(sxp *def, bool stochastic);

typedef struct t_lsystem {
  sxp *axiom;
  std::vector<production *> productions;
} lsystem;

lsystem *ls_load(char *file);
sxp *ls_apply(lsystem *ls, sxp *state);
sxp *ls_run(lsystem *ls, int n);
void dump_lsystem(lsystem *ls);

/* functions that are really only used within lsystems.cc */
typedef std::map<std::string, double> env;

sxp *ls_eval_expr(env *e, sxp *expr);
sxp *ls_eval_add(env *e, sxp *expr);
sxp *ls_eval_sub(env *e, sxp *expr);
sxp *ls_eval_mul(env *e, sxp *expr);
sxp *ls_eval_div(env *e, sxp *expr);
sxp *ls_eval_eq(env *e, sxp *expr);
sxp *ls_eval_lt(env *e, sxp *expr);
sxp *ls_eval_gt(env *e, sxp *expr);
sxp *ls_eval_lte(env *e, sxp *expr);
sxp *ls_eval_gte(env *e, sxp *expr);
sxp *ls_eval_and(env *e, sxp *expr);
sxp *ls_eval_or(env *e, sxp *expr);
sxp *ls_eval_not(env *e, sxp *expr);

#endif
