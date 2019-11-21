#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>


int
main(int argc,
     char *argv[]) {
  int i, j, n = 1;
  char buf[65536];

  
  if (argc > 1)
    n = atoi(argv[1]);

  for (i = 0; i < n; i++) {
    struct passwd pbuf, *pp = NULL;
    
    for (j = 2; j < argc; j++) {
      if (getpwnam_r(argv[j], &pbuf, buf, sizeof(buf), &pp) < 0) {
	fprintf(stderr, "%s: getpwnam_r(\"%s\") failed: %s\n", argv[0], argv[j], strerror(errno));
	exit(1);
      }
    }
  }
  
  return 0;
}


