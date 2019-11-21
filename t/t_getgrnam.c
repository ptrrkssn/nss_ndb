#include <grp.h>

int
main(int argc,
     char *argv[]) {
  int i, n = 1;

  if (argc > 1)
    n = atoi(argv[2]);

  for (i = 0; i < n; i++) {
    struct group *gp = getgrnam(argv[1]);
    sleep(1);
    printf("%d/%d\tgp = %p\n", i, n, gp);
  }
  (void) getgrnam("wheel");
}


