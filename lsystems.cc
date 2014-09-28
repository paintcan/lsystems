#include "lsystems.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>

static FILE *reading = 0;

static int reader() {
  if (!reading)
    return -1;
  int c = fgetc(reading);
  if (c == EOF) {
    fclose(reading);
    return -1;
  } return c;
}

stochastic_expansion *parse_stochastic_expansion(sxp *def) {
  assert(def->type == ty_integer || def->type == ty_float);
  
  stochastic_expansion *e = new stochastic_expansion;
  e->probability = def->type == ty_float ? def->R : def->Z;
  e->expansion = def->next;

  def = def->next;
  while (def) {
    sxp_assert_type(def, ty_sxp);
    def = def->next;
  } return e;
}

/* pass in pointer to production expression */
production *parse_production(sxp *def, bool stochastic) {
  assert(sxp_length(def) >= 2); /* allow empty expansion */
  sxp_assert_type(def, ty_sxp);

  production *p = new production;

  /* parse the matching pattern */
  sxp *t = def->down;
  int units = 0;
  int seglen = 0;
  std::vector<sxp *> accum;

  while (t) {
    switch (t->type) {
    case ty_sxp:
      accum.push_back(t->down);
      ++seglen;
      break;
    case ty_symbol:
      if (!strcmp(t->symbol, "<") && units == 0) {
	p->left = accum;
	accum = std::vector<sxp *>();
	seglen = 0;
	++units;
      } else if (!strcmp(t->symbol, ">") && units == 1) {
	if (seglen != 1) {
	  fprintf(stderr, "parse_production: center must be 1 symbol long\n");
	  exit(-1);
	} 
	p->center = accum[0];
	accum = std::vector<sxp *>();
	seglen = 0;
	++units;
      } break;
    default:
      fprintf(stderr, "parse_production: malformed production\n");
      exit(-1);
    } t = t->next;
  }
  
  if (units == 0 && seglen == 1) {
    p->center = accum[0]; 
  } else if (units == 1 && seglen == 1) {
    p->center = accum[0];
  } else if (units == 2) {
    p->right = accum;
  } else {
    fprintf(stderr, "parse_production: malformed production\n");
    exit(-1);
  }

  def = def->next;
  sxp_assert_type(def, ty_sxp);
  p->condition = def->down;
  def = def->next;

  if (stochastic) {
    if (!def) {
      fprintf(stderr, "parse_production: stochastic production has no expansion\n");
      exit(-1);
    }

    float prob = 0;
    while (def) {
      sxp_assert_type(def, ty_sxp);
      stochastic_expansion *r = parse_stochastic_expansion(def->down);
      p->expansion.push_back(r);
      def = def->next;
      prob += r->probability;
    }

    if (prob > 1)
      fprintf(stderr, "warning: encountered stochastic production with probabilities summing over 1\n");
  } else {
    stochastic_expansion *r = new stochastic_expansion;
    r->probability = 1;
    r->expansion = def;
    p->expansion.push_back(r);
    while (def) {
      sxp_assert_type(def, ty_sxp);
      def = def->next;
    } 
  }

  return p;
}

/* load up lsystem definition from file */
lsystem *ls_load(char *file) {
  reading = fopen(file, "rb");
  if (!reading) {
    fprintf(stderr, "couldn't read lsystem definition from %s\n", file);
    return 0;
  } set_reader(reader);
  
  lsystem *ls = new lsystem;
  sxp *def = sxp_next();

  while (def) {
    sxp_assert_type(def, ty_sxp);
    sxp_assert_type(def->down, ty_symbol);
    char *s = def->down->symbol;

    if (!strcmp(s, "axiom"))
      ls->axiom = def->down->next;
    else if (!strcmp(s, "production"))
      ls->productions.push_back(parse_production(def->down->next, false));
    else if (!strcmp(s, "stochastic-production"))
      ls->productions.push_back(parse_production(def->down->next, true));
    else {
      fprintf(stderr, "ls_load: %s is malformed\n", file);
      exit(-1);
    } def = def->next;
  } return ls;
}

static void dump_production(production *p) {
  printf("production left rules -\n");
  std::vector<sxp *>::iterator i = p->left.begin();
  while (i != p->left.end()) {
    printf("\t");
    sxp_print(*i);
    printf("\n");
    ++i;
  }
  printf("production central rule: "); sxp_print(p->center); printf("\n");

  i = p->right.begin();
  printf("production right rules -\n");
  while (i != p->right.end()) {
    printf("\t");
    sxp_print(*i);
    printf("\n");
    ++i;
  }

  printf("production condition: "); sxp_print(p->condition); printf("\n");
  printf("production expansions: \n");
  
  std::vector<stochastic_expansion *>::iterator j = p->expansion.begin();
  while (j != p->expansion.end()) {
    stochastic_expansion *e = *j;
    printf("probability %f\t", e->probability);
    printf("expansion "); sxp_print(e->expansion); printf("\n");
    ++j;
  }
}

void dump_lsystem(lsystem *ls) {
  std::vector<production *>::iterator i = ls->productions.begin();
  while (i != ls->productions.end()) {
    dump_production(*i);
    ++i;
  }

  printf("l-system axiom: "); sxp_print(ls->axiom); printf("\n");
}

/* evaluate various operators */

sxp *ls_eval_add(env *e, sxp *expr) {
  double result = 0;
  sxp *t;

  expr = expr->next;
  while (expr) {
    switch (expr->type) {
    case ty_integer:
      result += (double) expr->Z;
      break;

    case ty_float:
      result += expr->R;
      break;

    case ty_sxp:
      t = ls_eval_expr(e, expr->down);
      assert(t->type == ty_float);
      result += t->R;
      break;

    case ty_symbol:
      assert(e->find(expr->symbol) != e->end());
      result += (*e)[expr->symbol];
      break;
    }

    expr = expr->next;
  } return sxp_makefloat(result, 0);
}

sxp *ls_eval_sub(env *e, sxp *expr) {
  bool set = false;
  double result = 0;
  sxp *t;

  expr = expr->next;
  while (expr) {
    switch (expr->type) {
    case ty_integer:
      if (!set) {
	result = expr->Z;
	set = true;
      } else
	result -= (double) expr->Z;
      break;

    case ty_float:
      if (!set) {
	result = expr->R;
	set = true;
      } else
	result -= expr->R;
      break;

    case ty_sxp:
      t = ls_eval_expr(e, expr->down);
      assert(t->type == ty_float);

      if (!set) {
	result = t->R;
	set = true;
      } else
	result -= t->R;
      break;

    case ty_symbol:
      assert(e->find(expr->symbol) != e->end());

      if (!set) {
	result = (*e)[expr->symbol];
	set = true;
      } else
	result -= (*e)[expr->symbol];
      break;
    }

    expr = expr->next;
  } return sxp_makefloat(result, 0);
}

sxp *ls_eval_mul(env *e, sxp *expr) {
  double result = 1;
  sxp *t;

  expr = expr->next;
  while (expr) {
    switch (expr->type) {
    case ty_integer:
      result *= (double) expr->Z;
      break;

    case ty_float:
      result *= expr->R;
      break;

    case ty_sxp:
      t = ls_eval_expr(e, expr->down);
      assert(t->type == ty_float);
      result *= t->R;
      break;

    case ty_symbol:
      assert(e->find(expr->symbol) != e->end());
      result *= (*e)[expr->symbol];
      break;
    }

    expr = expr->next;
  } return sxp_makefloat(result, 0);
}

sxp *ls_eval_div(env *e, sxp *expr) {
  bool set = false;
  double result = 0;
  sxp *t;

  expr = expr->next;
  while (expr) {
    switch (expr->type) {
    case ty_integer:
      if (!set) {
	result = expr->Z;
	set = true;
      } else
	result /= (double) expr->Z;
      break;

    case ty_float:
      if (!set) {
	result = expr->R;
	set = true;
      } else
	result /= expr->R;
      break;

    case ty_sxp:
      t = ls_eval_expr(e, expr->down);
      assert(t->type == ty_float);

      if (!set) {
	result = t->R;
	set = true;
      } else
	result /= t->R;
      break;

    case ty_symbol:
      assert(e->find(expr->symbol) != e->end());

      if (!set) {
	result = (*e)[expr->symbol];
	set = true;
      } else
	result /= (*e)[expr->symbol];
      break;
    }

    expr = expr->next;
  } return sxp_makefloat(result, 0);
}

sxp *ls_eval_eq(env *e, sxp *expr) {
  sxp *t;
  expr = expr->next;
  assert(sxp_length(expr) == 2);
  
  double a, b;

  switch(expr->type) {
  case ty_integer:
    a = expr->Z;
    break;
  case ty_float:
    a = expr->R;
    break;
  case ty_symbol:
    assert(e->find(expr->symbol) != e->end());
    a = (*e)[expr->symbol];
    break;
  case ty_sxp:
    t = ls_eval_expr(e, expr->down);
    assert(t->type == ty_float);
    a = t->R;
    break;
  }
  
  expr = expr->next;
  switch(expr->type) {
  case ty_integer:
    b = expr->Z;
    break;
  case ty_float:
    b = expr->R;
    break;
  case ty_symbol:
    assert(e->find(expr->symbol) != e->end());
    b = (*e)[expr->symbol];
    break;
  case ty_sxp:
    t = ls_eval_expr(e, expr->down);
    assert(t->type == ty_float);
    b = t->R;
    break;
  }

  return sxp_makefloat(a == b ? 1 : 0, 0);
}

sxp *ls_eval_lt(env *e, sxp *expr) {
  sxp *t;
  expr = expr->next;
  assert(sxp_length(expr) == 2);
  
  double a, b;

  switch(expr->type) {
  case ty_integer:
    a = expr->Z;
    break;
  case ty_float:
    a = expr->R;
    break;
  case ty_symbol:
    assert(e->find(expr->symbol) != e->end());
    a = (*e)[expr->symbol];
    break;
  case ty_sxp:
    t = ls_eval_expr(e, expr->down);
    assert(t->type == ty_float);
    a = t->R;
    break;
  }
  
  expr = expr->next;
  switch(expr->type) {
  case ty_integer:
    b = expr->Z;
    break;
  case ty_float:
    b = expr->R;
    break;
  case ty_symbol:
    assert(e->find(expr->symbol) != e->end());
    b = (*e)[expr->symbol];
    break;
  case ty_sxp:
    t = ls_eval_expr(e, expr->down);
    assert(t->type == ty_float);
    b = t->R;
    break;
  }

  return sxp_makefloat(a < b ? 1 : 0, 0);
}

sxp *ls_eval_gt(env *e, sxp *expr) {
  sxp *t;
  expr = expr->next;
  assert(sxp_length(expr) == 2);
  
  double a, b;

  switch(expr->type) {
  case ty_integer:
    a = expr->Z;
    break;
  case ty_float:
    a = expr->R;
    break;
  case ty_symbol:
    assert(e->find(expr->symbol) != e->end());
    a = (*e)[expr->symbol];
    break;
  case ty_sxp:
    t = ls_eval_expr(e, expr->down);
    assert(t->type == ty_float);
    a = t->R;
    break;
  }
  
  expr = expr->next;
  switch(expr->type) {
  case ty_integer:
    b = expr->Z;
    break;
  case ty_float:
    b = expr->R;
    break;
  case ty_symbol:
    assert(e->find(expr->symbol) != e->end());
    b = (*e)[expr->symbol];
    break;
  case ty_sxp:
    t = ls_eval_expr(e, expr->down);
    assert(t->type == ty_float);
    b = t->R;
    break;
  }

  return sxp_makefloat(a > b ? 1 : 0, 0);
}


sxp *ls_eval_lte(env *e, sxp *expr) {
  sxp *t;
  expr = expr->next;
  assert(sxp_length(expr) == 2);
  
  double a, b;

  switch(expr->type) {
  case ty_integer:
    a = expr->Z;
    break;
  case ty_float:
    a = expr->R;
    break;
  case ty_symbol:
    assert(e->find(expr->symbol) != e->end());
    a = (*e)[expr->symbol];
    break;
  case ty_sxp:
    t = ls_eval_expr(e, expr->down);
    assert(t->type == ty_float);
    a = t->R;
    break;
  }
  
  expr = expr->next;
  switch(expr->type) {
  case ty_integer:
    b = expr->Z;
    break;
  case ty_float:
    b = expr->R;
    break;
  case ty_symbol:
    assert(e->find(expr->symbol) != e->end());
    b = (*e)[expr->symbol];
    break;
  case ty_sxp:
    t = ls_eval_expr(e, expr->down);
    assert(t->type == ty_float);
    b = t->R;
    break;
  }

  return sxp_makefloat(a <= b ? 1 : 0, 0);
}

sxp *ls_eval_gte(env *e, sxp *expr) {
  sxp *t;
  expr = expr->next;
  assert(sxp_length(expr) == 2);
  
  double a, b;

  switch(expr->type) {
  case ty_integer:
    a = expr->Z;
    break;
  case ty_float:
    a = expr->R;
    break;
  case ty_symbol:
    assert(e->find(expr->symbol) != e->end());
    a = (*e)[expr->symbol];
    break;
  case ty_sxp:
    t = ls_eval_expr(e, expr->down);
    assert(t->type == ty_float);
    a = t->R;
    break;
  }
  
  expr = expr->next;
  switch(expr->type) {
  case ty_integer:
    b = expr->Z;
    break;
  case ty_float:
    b = expr->R;
    break;
  case ty_symbol:
    assert(e->find(expr->symbol) != e->end());
    b = (*e)[expr->symbol];
    break;
  case ty_sxp:
    t = ls_eval_expr(e, expr->down);
    assert(t->type == ty_float);
    b = t->R;
    break;
  }

  return sxp_makefloat(a >= b ? 1 : 0, 0);
}

sxp *ls_eval_and(env *e, sxp *expr) {
  sxp *t;

  expr = expr->next;
  while (expr) {
    switch (expr->type) {
    case ty_integer:
      if(t->Z == 0)
	return sxp_makefloat(0, 0);
      break;

    case ty_float:
      if (t->R == 0)
	return sxp_makefloat(0, 0);
      break;

    case ty_sxp:
      t = ls_eval_expr(e, expr->down);
      assert(t->type == ty_float);
      if (t->R == 0)
	return sxp_makefloat(0, 0);
      break;

    case ty_symbol:
      assert(e->find(expr->symbol) != e->end());
      if((*e)[expr->symbol] == 0)
	return sxp_makefloat(0, 0);
      break;
    }

    expr = expr->next;
  } return sxp_makefloat(1, 0);
}

sxp *ls_eval_or(env *e, sxp *expr) {
  sxp *t;

  expr = expr->next;
  while (expr) {
    switch (expr->type) {
    case ty_integer:
      if(t->Z == 1)
	return sxp_makefloat(1, 0);
      break;

    case ty_float:
      if (t->R == 1)
	return sxp_makefloat(1, 0);
      break;

    case ty_sxp:
      t = ls_eval_expr(e, expr->down);
      assert(t->type == ty_float);
      if (t->R == 1)
	return sxp_makefloat(1, 0);
      break;

    case ty_symbol:
      assert(e->find(expr->symbol) != e->end());
      if((*e)[expr->symbol] == 1)
	return sxp_makefloat(1, 0);
      break;
    }

    expr = expr->next;
  } return sxp_makefloat(0, 0);
}

sxp *ls_eval_not(env *e, sxp *expr) {
  sxp *t;
  expr = expr->next;
  assert(sxp_length(expr) == 1);
  
  double a;
  switch(expr->type) {
  case ty_integer:
    a = expr->Z;
    break;
  case ty_float:
    a = expr->R;
    break;
  case ty_symbol:
    assert(e->find(expr->symbol) != e->end());
    a = (*e)[expr->symbol];
    break;
  case ty_sxp:
    t = ls_eval_expr(e, expr->down);
    assert(t->type == ty_float);
    a = t->R;
    break;
  }

  return sxp_makefloat(a == 0 ? 1 : 0, 0);
}

/* evaluate expression given set of symbolic bindings */
sxp *ls_eval_expr(env *e, sxp *expr) {
  if (expr == 0)
    return 0;
  if (expr->type == ty_sxp)
    return sxp_makesxp(ls_eval_expr(e, expr->down), 
		       ls_eval_expr(e, expr->next));

  sxp_assert_type(expr, ty_symbol);
  char *s = expr->symbol;

  /* operators return a value wrapped in a sxp node */
  if (!strcmp(s, "+")) {
    return ls_eval_add(e, expr);
  } else if (!strcmp(s, "-")) {
    return ls_eval_sub(e, expr);
  } else if (!strcmp(s, "*")) {
    return ls_eval_mul(e, expr);
  } else if (!strcmp(s, "/")) {
    return ls_eval_div(e, expr);
  } else if (!strcmp(s, "<")) {
    return ls_eval_lt(e, expr);
  } else if (!strcmp(s, ">")) {
    return ls_eval_gt(e, expr);
  } else if (!strcmp(s, "<=")) {
    return ls_eval_lte(e, expr);
  } else if (!strcmp(s, ">=")) {
    return ls_eval_gte(e, expr);
  } else if (!strcmp(s, "=")) {
    return ls_eval_eq(e, expr);
  } else if (!strcmp(s, "and")) {
    return ls_eval_and(e, expr);
  } else if (!strcmp(s, "or")) {
    return ls_eval_or(e, expr);
  } else if (!strcmp(s, "not")) {
    return ls_eval_not(e, expr);
  }

  /* if the first symbol is not a known operator, then it is considered
     to be an alphabet symbol. the remainder of the symbols in the expression
     are evaluated */
  
  sxp *rv, *nv = sxp_makesymbol(s, 0);
  rv = nv;
  
  expr = expr->next;
  while (expr) {
    if (expr->type == ty_symbol) {
      /* symbolic types are substituted with a value from the map if possible */
      if (e->find(expr->symbol) != e->end())
	nv->next = sxp_makefloat((*e)[expr->symbol], 0);
      else 
	nv->next = sxp_makesymbol(expr->symbol, 0);
    } else if (expr->type == ty_sxp) {
      /* eval_expr will either return an evaluated subexpression or a float */
      sxp *t = ls_eval_expr(e, expr->down);
      if (t->type == ty_symbol) {
	nv->next = sxp_makesxp(t, 0);
      } else 
	nv->next = t;
    } else if (expr->type == ty_integer) {
      nv->next = sxp_makeint(expr->Z, 0);
    } else if (expr->type == ty_float) {
      nv->next = sxp_makefloat(expr->R, 0);
    } else {
      fprintf(stderr, "ls_eval_expr: encountered unexpected type\n");
      exit(-1);
    } 

    nv = nv->next;
    expr = expr->next;
  }

  return rv;
}

bool attempt_matcher(env *e, sxp *rule, sxp *src) {
  if (rule == 0 && src == 0)
    return true;
  if (rule == 0 || src == 0)
    return false;
  assert(rule->type != ty_sxp && src->type != ty_sxp);

  float a, b;
  switch (rule->type) {
  case ty_integer:
    a = rule->Z;
    if (src->type != ty_integer || src->type != ty_float)
      return false;
    if (src->type == ty_integer)
      b = src->Z;
    else
      b = src->R;
    if (a != b)
      return false;
    break;
  case ty_float:
    a = rule->R;
    if (src->type != ty_integer || src->type != ty_float)
      return false;
    if (src->type== ty_integer)
      b = src->Z;
    else
      b = src->R;
    if (a != b)
      return false;
    break;
  case ty_symbol:
    switch(src->type) {
    case ty_symbol:
      if (strcmp(rule->symbol, src->symbol))
	return false;
      break;
    case ty_integer:
      if (e->find(rule->symbol) != e->end()) {
	if ((*e)[rule->symbol] != src->Z)
	  return false;
      } else
	(*e)[rule->symbol] = src->Z;
      break;
    case ty_float:
      if (e->find(rule->symbol) != e->end()) {
	if ((*e)[rule->symbol] != src->R)
	  return false;
      } else
	(*e)[rule->symbol] = src->R;
      break;
    } break;
  }

  return attempt_matcher(e, rule->next, src->next);
}

bool attempt_match_first(sxp *rule, sxp *src) {
  sxp_assert_type(rule, ty_symbol);
  sxp_assert_type(src, ty_symbol);

  if (!strcmp(rule->symbol, src->symbol))
    return true;
  return false;
}

env *attempt_match(production *p, std::vector<sxp *> &in, int pos) {
  env *e = new env;
  int idx = pos - p->left.size();
  
  for (int i = 0; i < p->left.size(); i++) {
    if (!attempt_match_first(p->left[i], in[idx])
	|| !attempt_matcher(e, p->left[i], in[idx])) {
      delete e;
      return 0;
    } ++idx; 
  }

  if (!attempt_match_first(p->center, in[idx])
      || !attempt_matcher(e, p->center, in[idx])) {
    delete e;
    return 0;
  } ++idx;

  for (int i = 0; i < p->right.size(); i++) {
    if (!attempt_match_first(p->right[i], in[idx])
	|| !attempt_matcher(e, p->right[i], in[idx])) {
      delete e;
      return 0;
    } ++idx;
  } 

  return e;
}

sxp *ls_apply(lsystem *ls, sxp *state) {
  std::vector<sxp *> input;
  std::vector<sxp *> output;
  
  sxp *start = state;
  while (state) {
    sxp_assert_type(state, ty_sxp);
    /* skip branches when matching */
    if (state->down->type != ty_sxp)
      input.push_back(state->down);
    state = state->next;
  }

  int sz = input.size();
  for (int i = 0; i < sz; i++) {
    bool applied_production = false;
    std::vector<production *>::iterator j = ls->productions.begin();
    while (j != ls->productions.end() && !applied_production) {
      production *p = *j;
      
      /* can this production apply? */
      if (p->left.size() <= i && i+1+p->right.size() <= sz) {
	env *e = attempt_match(p, input, i);
	if (e) {
	  /* test condition */
	  sxp *test = p->condition ? ls_eval_expr(e, p->condition) : 0;
	  bool matches = false;
	  if (test) {
	    assert(test->type == ty_float || test->type == ty_integer);
	    switch (test->type) {
	    case ty_float:
	      if (test->R > 0)
		matches = true;
	      break;
	    case ty_integer:
	      if (test->Z > 0)
		matches = true;
	      break;
	    }
	  } else
	    matches = true; /* empty condition */

	  if (matches) {
	    double prob = (double) (rand() % 1000000) / 1000000.0;
	    double sum = 0;

	    /* figure out which expansion to apply */
	    sxp *r = 0;
	    std::vector<stochastic_expansion *>::iterator k = p->expansion.begin();
	    while (k != p->expansion.end()) {
	      stochastic_expansion *exp = *k;
	      sum += exp->probability;
	      if (prob < sum) {
		r = exp->expansion;
		break;
	      } ++k;
	    }

	    /* compute expansion */
	    sxp *o = 0, *s = 0;
	    while (r) {
	      if (o) {
		o->next = sxp_makesxp(ls_eval_expr(e, r->down), 0);
		o = o->next;
	      } else {
		o = sxp_makesxp(ls_eval_expr(e, r->down), 0);
		s = o;
	      }
	      r = r->next;
	    }
	    
	    output.push_back(s);
	    applied_production = true;
	  } 

	  delete e;
	}
      } ++j;
    }

    /* if no productions applied, preserve input */
    if (!applied_production)
      output.push_back(sxp_makesxp(input[i], 0));
  }

  if (output.size() == 0)
    return 0;

  /* stitch together output */
  sxp *o = 0, *s = 0;
  while (start && start->down->type == ty_sxp) {
    if (o == 0) {
      o = sxp_makesxp(ls_apply(ls, start->down), 0);
      s = o;
    } else {
      o->next = sxp_makesxp(ls_apply(ls, start->down),0);
      o = o->next;
    } start = start->next;
  }

  if (!o) {
    o = output[0];
    s = o;
  } else
    o->next = output[0];
  while (o && o->next)
    o = o->next;
  start = start->next;

  for (int i = 1; i < output.size(); i++) {
    while (start && start->down->type == ty_sxp) {
      if (o == 0) {
	o = sxp_makesxp(ls_apply(ls, start->down), 0);
	s = o;
      } else {
	o->next = sxp_makesxp(ls_apply(ls, start->down),0);
	o = o->next;
      } start = start->next;
    }

    if (!o) {
      o = output[i];
      s = o;
    } else
      o->next = output[i];
    while (o && o->next)
      o = o->next;
    start = start->next;
  } return s;
}

sxp *ls_runner(lsystem *ls, sxp *words, int n) {
  if (n <= 0)
    return words;
  return ls_runner(ls, ls_apply(ls, words), n-1);
}

sxp *ls_run(lsystem *ls, int n) {
  return ls_runner(ls, ls->axiom, n);
}
