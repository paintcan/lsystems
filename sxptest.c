#include "sexp.h"
#include <stdio.h>

char *test = "(A (x y z) (and (> x 10) (< z 5))) (qqq)";

int read(void) {
  static char *p = 0;
  if (!p)
    p = test;
  if (!*p)
    return -1;
  return *p++;
}

int main() {
  set_reader(read);
  sxp *r = sxp_next();
  sxp_print(r); printf("\n");
  sxp_dest(r);
  
  return 0;
}
