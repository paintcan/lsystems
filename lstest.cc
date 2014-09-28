#include <iostream>
#include "lsystems.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("usage: lstest [definitions] [generations]\n");
    return 0;
  }
  
  int ngen = atoi(argv[2]);
  lsystem *l = ls_load(argv[1]);
  if (!l)
    return -1;
  
  if (ngen < 0) {
    printf("positive generations only please.\n");
    return -2;
  }

  dump_lsystem(l);
  long seed = time(0);
  printf("%d generations of evolution:\n\n", ngen);
  for (int i = 0; i < ngen; i++) {
    srand(seed);
    sxp_print(ls_run(l, i)); printf("\n\n");
  }

  return 0;
}
