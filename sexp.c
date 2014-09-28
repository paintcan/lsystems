#include "sexp.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* memory management */

sxp *sxp_makeint(int Z, sxp *n) {
  sxp *s = (sxp *) malloc(sizeof(sxp));
  s->type = ty_integer;
  s->Z = Z;
  s->next = n;
  return s;
}

sxp *sxp_makefloat(double R, sxp *n) {
  sxp *s = (sxp *) malloc(sizeof(sxp));
  s->type = ty_float;
  s->R = R;
  s->next = n;
  return s;
}

sxp *sxp_makesymbol(char *sym, sxp *n) {
  sxp *s = (sxp *) malloc(sizeof(sxp));
  s->type = ty_symbol;
  s->symbol = strdup(sym);
  s->next = n;
  return s;
}

sxp *sxp_makesxp(sxp *d, sxp *n) {
  sxp *s = (sxp *) malloc(sizeof(sxp));
  s->type = ty_sxp;
  s->down = d;
  s->next = n;
  return s;
}

void sxp_dest(sxp *x) {
  sxp *t;
  while (x) {
    t = x->next;

    switch (x->type) {
    case ty_symbol:
      free(x->symbol);
      break;
    case ty_sxp:
      sxp_dest(x->down);
      break;
    } free(x);

    x = t;
  } 
}

/* print for debugging purposes */

void sxp_print(sxp *x) {
  while (x) {
    switch (x->type) {
    case ty_sxp:
      printf(" (");
      sxp_print(x->down);
      printf(" )");
      break;
    case ty_integer:
      printf(" int:%d", x->Z);
      break;
    case ty_float:
      printf(" float:%f", x->R);
      break;
    case ty_symbol:
      printf(" %s", x->symbol);
      break;
    } x = x->next;
  } 
}

/* parsing code */

static int dummy_reader() {
  return -1;
}

static int (*readchar)(void) = dummy_reader;

void set_reader(int (*read)(void)) {
  readchar = read;
}

static int is_item(int c) {
  if ((isalnum(c) || ispunct(c)) && c != '(' && c != ')')
    return 1;
  return 0;
}

static sxp *sxp_parse_item(char *buf) {
  char *p = buf;
  int type = ty_integer;

  while (*p) {
    if (!isdigit(*p)) {
      type = ty_float;
      if (*p != '.') {
	type = ty_symbol;
	break;
      }
    } ++p;
  }

  switch (type) {
  case ty_integer:
    return sxp_makeint(atoi(buf), 0);
  case ty_float:
    return sxp_makefloat(atof(buf), 0);
  case ty_symbol:
    return sxp_makesymbol(buf, 0);
  }
}

sxp *sxp_next() {
  int parse = 1;
  int c;

  sxp *start_fragment = 0;
  sxp *current = 0, *t;
  char buf[256];
  
  c = readchar();
  while (parse) {
    t = 0;
    while (c != -1 && isspace(c))
      c = readchar();

    if (c == -1)
      parse = 0;
    else if (c == '(') {
      t = sxp_makesxp(sxp_next(), 0);
      c = readchar();
    } else if (c == ')')
      return start_fragment;
    else if (c == ';') {
      while (c != -1 && c != '\n')
	c = readchar();
      continue;
    }
    else if (is_item(c)) {
      char *p = buf;
      while (is_item(c)) {
	*p++ = c;
	c = readchar();
      } *p = 0;
      t = sxp_parse_item(buf);      
    } else {
      fprintf(stderr, "sxp_next encountered unexpected character\n");
      exit(-1);
    }

    if (current == 0) {
      start_fragment = current = t;
    } else {
      current->next = t;
      current = t;
    }
  }

  return start_fragment;
}

int sxp_isequal(sxp *a, sxp *b) {
  if (a == b)
    return 1;
  if (a == 0 || b == 0)
    return 0;
  if (a->type != b->type)
    return 0;

  switch (a->type) {
  case ty_sxp:
    if (!sxp_isequal(a->down, b->down))
      return 0;
    break;
  case ty_integer:
    if (a->Z != b->Z)
      return 0;
    break;
  case ty_float:
    if (a->R != b->R)
      return 0;
    break;
  case ty_symbol:
    if (strcmp(a->symbol, b->symbol))
      return 0;
    break;
  } return sxp_isequal(a->next, b->next);
}

int sxp_length(sxp *s) {
  int len = 0;
  while (s) {
    s = s->next;
    ++len;
  } return len;
}

static char *sxp_type_string(int type) {
  switch (type) {
  case ty_float:
    return "float";
  case ty_integer:
    return "integer";
  case ty_symbol:
    return "symbol";
  case ty_sxp:
    return "sxp";
  } return "[error]";
}


/*
void sxp_assert_type(sxp *x, int type) {
  if (!x) {
    fprintf(stderr, "sxp_assert_type: unexpected null node encountered (wanted type %s)\n", sxp_type_string(type));
    exit(-1);
  }

  if (x->type != type) {
    fprintf(stderr, "sxp_assert_type: wanted type %s, got type %s\n", 
	    sxp_type_string(type), sxp_type_string(x->type));
    exit(-1);
  }
}
*/
