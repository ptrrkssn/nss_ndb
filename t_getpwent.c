#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>

char buf[2*1024*1024];

int
main(int argc,
     char *argv[]) {
  int i, j, rc, n = 1;
  
  if (argc > 1)
    n = atoi(argv[1]);


  for (i = 0; i < n; i++) {
    struct passwd pbuf, *pp = NULL;

    setpwent();
    
    j = 0;
    while ((rc = getpwent_r(&pbuf, buf, sizeof(buf), &pp)) == 0 && pp)
      j++;

    if (rc < 0) {
      fprintf(stderr, "%s: getpwent_r() failed: %s\n", argv[0], strerror(errno));
      exit(1);
    }

    endpwent();
    printf("%d/%d: %d entries\n", i, n, j);
  }

  return 0;
}


