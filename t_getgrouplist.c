#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#define MAXGROUPS 1024


int
main(int argc,
     char *argv[]) {
  int i, j, n = 1;

  
  if (argc > 1)
    n = atoi(argv[1]);
  
  for (i = 0; i < n; i++) {
    for (j = 2; j < argc; j++) {
      struct passwd *pp;
    
      gid_t gidv[MAXGROUPS];
      int ngv = MAXGROUPS;

      pp = getpwnam(argv[j]);
	
      if (getgrouplist(argv[j], pp ? pp->pw_gid : -1, &gidv[0], &ngv) < 0) {
	fprintf(stderr, "%s: getgrouplist(\"%s\") failed: %s\n", argv[0], argv[j], strerror(errno));
	exit(1);
      }

      printf("%d/%d: %s: %d groups\n", i, n, argv[j], ngv);
    }
  }
  
  return 0;
}


