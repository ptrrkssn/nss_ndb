#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <grp.h>

char buf[8*1024*1024];

int
main(int argc,
     char *argv[]) {
  int i, j, rc, n = 1;

  
  if (argc > 1)
    n = atoi(argv[1]);

  for (i = 0; i < n; i++) {
    struct group gbuf, *gp = NULL;

    setgrent();
    
    j = 0;
    while ((rc = getgrent_r(&gbuf, buf, sizeof(buf), &gp)) == 0 && gp)
      j++;

    if (rc < 0) {
      fprintf(stderr, "%s: getgrent_r() failed: %s\n", argv[0], strerror(errno));
      exit(1);
    }

    endgrent();
  }

  return 0;
}


