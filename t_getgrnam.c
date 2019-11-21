#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <grp.h>

char buf[8*1024*1024];

int
main(int argc,
     char *argv[]) {
  int i, j, n = 1;

  
  if (argc > 1)
    n = atoi(argv[1]);
  
  for (i = 0; i < n; i++) {
    struct group gbuf, *gp = NULL;

    for (j = 2; j < argc; j++) {
      if (getgrnam_r(argv[j], &gbuf, buf, sizeof(buf), &gp) < 0) {
	fprintf(stderr, "%s: getgrnam_r(\"%s\") failed: %s\n", argv[0], argv[j], strerror(errno));
	exit(1);
      }
    }
  }
  
  return 0;
}


