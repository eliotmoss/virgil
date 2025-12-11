# include <stdio.h>
# include <stdlib.h>
extern void *getIncr(void *);

int main(int argc, char *argv[]) {
  int i = atoi(argv[1]);
  void *p = getIncr(NULL);
  int (*incr)(void *,int) = p;
  int j = incr(NULL, i - 1);
  printf("%d\n", j == 1 ? 89 : 75);
  return 0;
}
