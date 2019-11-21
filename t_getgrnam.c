#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <grp.h>

char *group = "wheel";



int
main(int argc,
     char *argv[]) {
  int i, n = 1;
  char buf[65536];

  
  if (argc > 1)
    group = argv[1];
  if (argc > 2)
    n = atoi(argv[2]);

  for (i = 0; i < n; i++) {
    struct group gbuf, *gp = NULL;

    if (getgrnam_r(group, &gbuf, buf, sizeof(buf), &gp) < 0) {
      fprintf(stderr, "%s: getgrnam_r(\"%s\") failed: %s\n", argv[0], group, strerror(errno));
      exit(1);
    }
		
    printf("%d/%d\tgp = %p\n", i, n, gp);
  }
  
  return 0;
}


