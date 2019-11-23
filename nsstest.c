/*
 * t_libc.c - Test passwd & group libc functionality
 *
 * Copyright (c) 2019 Peter Eriksson <pen@lysator.liu.se>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <pthread.h>


#define MAXGROUPS 1024


char *argv0 = "t_libc";

int n_repeat = 0;
int n_timeout = 2;
int n_threads = 0;
int n_bufsize = 1*1024*1024;

int f_verbose = 0;
int f_stayopen = 0;
int f_expfail = 0;


int
p_passwd(struct passwd *pp) {
  if (!pp) {
    puts("NULL");
    return -1;
  }
  
  printf("%s:%s:%d:%d:%s:%s:%s\n",
	 pp->pw_name ? pp->pw_name : "<null>",
	 pp->pw_passwd ? pp->pw_passwd : "<null>",
	 pp->pw_uid, pp->pw_gid,
	 pp->pw_gecos ? pp->pw_gecos : "<null>",
	 pp->pw_dir ?  pp->pw_dir : "<null>",
	 pp->pw_shell ? pp->pw_shell : "<null>");
  
  return pp->pw_name && pp->pw_passwd && pp->pw_gecos && pp->pw_dir && pp->pw_shell;
}


int
p_group(struct group *gp) {
  int i = 0;

  
  if (!gp) {
    puts("NULL");
    return -1;
  }
  
  printf("%s:%s:%d:",
	 gp->gr_name ? gp->gr_name : "<null>",
	 gp->gr_passwd ? gp->gr_passwd : "<null>",
	 gp->gr_gid);
  
  if(gp->gr_mem) {
    for (i=0; gp->gr_mem[i]; ++i) {
      if (i > 0)
	putchar(',');
      printf("%s", gp->gr_mem[i]);
    }
  } else {
    puts("NULL");
  }
  putchar('\n');
  
  return (gp->gr_name && gp->gr_passwd) ? i : 0;
}


double
d_time(struct timespec *t1,
       struct timespec *t2) {
  double dt1, dt2;

  dt1 = t1->tv_sec + (double) t1->tv_nsec / 1000000000.0;
  dt2 = t2->tv_sec + (double) t2->tv_nsec / 1000000000.0;

  return dt2-dt1;
}


char *
s_time(double dt) {
  static char tbuf[256];

  if (dt < 1.0) {
    dt *= 1000.0;
    if (dt < 1.0) {
      dt *= 1000.0;
      if (dt < 1.0) {
	dt *= 1000.0;
	sprintf(tbuf, "%.2f ns", dt);
      } else {
	sprintf(tbuf, "%.2f µs", dt);
      }
    } else {
      sprintf(tbuf, "%.2f ms", dt);
    }
  } else {
    if (dt > 60) {
      dt /= 60.0;
      if (dt > 60) {
	dt /= 60.0;
	sprintf(tbuf, "%.2f h", dt);
      } else {
	sprintf(tbuf, "%.2f m", dt);
      }
    } else {
      sprintf(tbuf, "%.2f s", dt);
    }
  }
  
  return tbuf;
}


int
t_getpwnam(int argc,
	   char *argv[],
	   void *xp,
	   unsigned long *ncp) {
  int i, rc = -1;

  
  for (i = 1; i < argc; i++) {
    struct passwd *pp = NULL;
    int trc = -1;

    
    errno = 0;
    pp = getpwnam(argv[i]);
    ++*ncp;

    if (!pp) {
      if (errno) {
	fprintf(stderr, "%s: Error: getpwnam(\"%s\") failed: %s\n",
		argv0, argv[i], strerror(errno));
	exit(1);
      }

      if (f_verbose)
	fprintf(stderr, "%s: Error: getpwnam(\"%s\"): User not found\n",
		argv0, argv[i]);
      trc = 1;
    } else {
      if (f_verbose)
	p_passwd(pp);
      trc = 0;
    }

    if (rc >= 0 && rc != trc) {
      fprintf(stderr, "%s: Error: getpwnam(\"%s\") not yielding similar result as previous\n",
	      argv0, argv[i]);
      exit(1);
    }

    rc = trc;
  }
  
  return rc;
}


int
t_getpwnam_r(int argc,
	     char *argv[],
	     void *xp,
	     unsigned long *ncp) {
  char *buf = (char *) xp;
  int i, rc = -1;

  
  for (i = 1; i < argc; i++) {
    struct passwd pbuf, *pp = NULL;
    int ec, trc = -1;
    

    ec = getpwnam_r(argv[i], &pbuf, buf, n_bufsize, &pp);
    if (ec) {
      fprintf(stderr, "%s: Error: getpwnam_r(\"%s\") failed: %s\n",
	      argv0, argv[i], strerror(ec));
      exit(1);
    }
    
    ++*ncp;
    
    if (!pp) {
      if (f_verbose)
	fprintf(stderr, "%s: Error: getpwnam_r(\"%s\"): User not found\n",
		argv0, argv[i]);
      trc = 1;
    } else {
      if (f_verbose)
	p_passwd(pp);
      trc = 0;
    }

    if (rc >= 0 && rc != trc) {
      fprintf(stderr, "%s: Error: getpwnam_r(\"%s\") not yielding similar result as previous\n",
	      argv0, argv[i]);
      exit(1);
    }
    
    rc = trc;
  }

  return rc;
}


int
t_getpwuid(int argc,
	   char *argv[],
	   void *xp,
	   unsigned long *ncp) {
  int i, rc = -1;

  
  for (i = 1; i < argc; i++) {
    struct passwd *pp = NULL;
    uid_t uid;
    int trc = -1;

    
    if (sscanf(argv[i], "%d", &uid) != 1) {
      fprintf(stderr, "%s: Error: %s: Invalid uid\n",
	      argv0, argv[i]);
      exit(1);
    }

    errno = 0;
    pp = getpwuid(uid);
    ++*ncp;
    
    if (!pp) {
      if (errno) {
	fprintf(stderr, "%s: Error: getpwuid(%d) failed: %s\n",
		argv0, uid, strerror(errno));
	exit(1);
      }

      if (f_verbose)
	fprintf(stderr, "%s: Error: getpwuid(%d): UID (User ID) not found\n",
		argv0, uid);
      trc = 1;
    } else {
      if (f_verbose)
	p_passwd(pp);
      trc = 0;
    }

    if (rc >= 0 && rc != trc) {
      fprintf(stderr, "%s: Error: getpwuid(%d) not yielding similar result as previous\n",
	      argv0, uid);
      exit(1);
    }
    
    rc = trc;
  }
  
  return rc;
}


int
t_getpwuid_r(int argc,
	     char *argv[],
	     void *xp,
	     unsigned long *ncp) {
  char *buf = (char *) xp;
  int i, rc = -1;

  
  for (i = 1; i < argc; i++) {
    struct passwd pbuf, *pp = NULL;
    uid_t uid;
    int ec, trc = -1;

    
    if (sscanf(argv[i], "%d", &uid) != 1) {
      fprintf(stderr, "%s: Error: %s: Invalid uid\n", argv0, argv[i]);
      exit(1);
    }
    
    ec = getpwuid_r(uid, &pbuf, buf, n_bufsize, &pp);
    if (ec) {
      fprintf(stderr, "%s: Error: getpwuid_r(%d) failed: %s\n", argv0, uid, strerror(ec));
      exit(1);
    }
    
    ++*ncp;
    
    if (!pp) {
      if (f_verbose)
	fprintf(stderr, "%s: Error: getpwuid_r(%d) failed: %s\n", argv0, uid, "UID (User ID) not found");
      trc = 1;
    } else {
      if (f_verbose)
	p_passwd(pp);
      trc = 0;
    }
    
    if (rc >= 0 && rc != trc) {
      fprintf(stderr, "%s: Error: getpwuid_r(%d) not yielding similar result as previous\n",
	      argv0, uid);
      exit(1);
    }
    
    rc = trc;
  }

  return rc;
}


int
t_getpwent(int argc,
	   char *argv[],
	   void *xp,
	   unsigned long *ncp) {
  struct passwd *pp = NULL;
  unsigned int m = 0;
  
  
  setpwent();

  errno = 0;
  while ((pp = getpwent()) != NULL) {
    ++*ncp;
    ++m;
    if (f_verbose)
      p_passwd(pp);
  }

  if (errno) {
    fprintf(stderr, "%s: Error: getpwent() failed: %s\n", argv0, strerror(errno));
    exit(1);
  }
  
  endpwent();

  if (!m) {
    if (f_verbose)
      fprintf(stderr, "%s: Error: getpwent(): No entries found\n", argv0);
    return 1;
  }
  
  return 0;
}


int
t_getpwent_r(int argc,
	     char *argv[],
	     void *xp,
	     unsigned long *ncp) {
  char *buf = (char *) xp;
  struct passwd pbuf, *pp = NULL;
  int ec;
  unsigned int m = 0;

  
  setpwent();
  
  while ((ec = getpwent_r(&pbuf, buf, n_bufsize, &pp)) == 0 && pp) {
    ++*ncp;
    ++m;
    
    if (f_verbose)
      p_passwd(pp);
  }
  
  if (ec) {
    fprintf(stderr, "%s: Error: getpwent_r() failed: %s\n",
	    argv0, strerror(ec));
    exit(1);
  }

  endpwent();
  
  if (!m) {
    if (f_verbose)
      fprintf(stderr, "%s: Error: getpwent_r(): No entries found\n", argv0);
    return 1;
  }
  
  return 0;
}


int
t_getgrnam(int argc,
	   char *argv[],
	   void *xp,
	   unsigned long *ncp) {
  int i, rc = -1;

  
  for (i = 1; i < argc; i++) {
    struct group *gp = NULL;
    int trc = -1;
    

    errno = 0;
    gp = getgrnam(argv[i]);
    ++*ncp;
    
    if (!gp) {
      if (errno) {
	fprintf(stderr, "%s: Error: getgrnam(\"%s\") failed: %s\n", argv0, argv[i], strerror(errno));
	exit(1);
      }

      if (f_verbose)
	fprintf(stderr, "%s: Error: getgrnam(\"%s\"): Group not found\n", argv0, argv[i]);
      trc = 1;
    } else {
      if (f_verbose)
	p_group(gp);
      trc = 0;
    }

    if (rc >= 0 && rc != trc) {
      fprintf(stderr, "%s: Error: getgrnam(\"%s\") not yielding similar result as previous\n",
	      argv0, argv[i]);
      exit(1);
    }

    rc = trc;
  }
  
  return rc;
}


int
t_getgrnam_r(int argc,
	     char *argv[],
	     void *xp,
	     unsigned long *ncp) {
  char *buf = (char *) xp;
  int i, rc = -1;

  
  for (i = 1; i < argc; i++) {
    struct group gbuf, *gp = NULL;
    int trc = -1;
    
    
    if (getgrnam_r(argv[i], &gbuf, buf, n_bufsize, &gp) < 0) {
      fprintf(stderr, "%s: Error: getgrnam_r(\"%s\") failed: %s\n", argv0, argv[i], strerror(errno));
      exit(1);
    }
    
    ++*ncp;
    
    if (!gp) {
      if (f_verbose)
	fprintf(stderr, "%s: Error: getgrnam_r(\"%s\"): Group not found\n", argv0, argv[i]);
      trc = 1;
    } else {
      if (f_verbose)
	p_group(gp);
      trc = 0;
    }

    if (rc >= 0 && rc != trc) {
      fprintf(stderr, "%s: Error: getgrnam_r(\"%s\") not yielding similar result as previous\n",
	      argv0, argv[i]);
      exit(1);
    }
    
    rc = trc;
  }

  return rc;
}


int
t_getgrgid(int argc,
	   char *argv[],
	   void *xp,
	   unsigned long *ncp) {
  int i, rc = -1;

  
  for (i = 1; i < argc; i++) {
    struct group *gp = NULL;
    gid_t gid;
    int trc = -1;

    
    if (sscanf(argv[i], "%d", &gid) != 1) {
      fprintf(stderr, "%s: Error: %s: Invalid gid\n", argv0, argv[i]);
      exit(1);
    }

    errno = 0;
    gp = getgrgid(gid);
    ++*ncp;
    
    if (!gp) {
      if (errno) {
	fprintf(stderr, "%s: Error: getgrgid(%d) failed: %s\n", argv0, gid, strerror(errno));
	exit(1);
      }

      if (f_verbose)
	fprintf(stderr, "%s: Error: getgrgid(%d): GID (Group ID) not found\n", argv0, gid);
      trc = 1;
    } else {
      if (f_verbose)
	p_group(gp);
      trc = 0;
    }

    if (rc >= 0 && rc != trc) {
      fprintf(stderr, "%s: Error: getgrgid(%d) not yielding similar result as previous\n",
	      argv0, gid);
      exit(1);
    }
    
    rc = trc;
  }
  
  return rc;
}


int
t_getgrgid_r(int argc,
	     char *argv[],
	     void *xp,
	     unsigned long *ncp) {
  char *buf = (char *) xp;
  int i, rc = -1;

  
  for (i = 1; i < argc; i++) {
    struct group gbuf, *gp = NULL;
    gid_t gid;
    int ec, trc = -1;
    
    
    if (sscanf(argv[i], "%d", &gid) != 1) {
      fprintf(stderr, "%s: Error: %s: Invalid gid\n", argv0, argv[i]);
      exit(1);
    }
    
    ec = getgrgid_r(gid, &gbuf, buf, n_bufsize, &gp);
    if (ec) {
      fprintf(stderr, "%s: Error: getgrgid_r(%d) failed: %s\n",
	      argv0, gid, strerror(ec));
      exit(1);
    }
    
    ++*ncp;
    
    if (!gp) {
      if (f_verbose)
	fprintf(stderr, "%s: Error: getgrgid_r(%d) failed: %s\n",
		argv0, gid, "GID (Group ID) not found");
      trc = 1;
    } else {
      if (f_verbose)
	p_group(gp);
      trc = 0;
    }
    
    if (rc >= 0 && rc != trc) {
      fprintf(stderr, "%s: Error: getgrgid_r(%d) not yielding similar result as previous\n",
	      argv0, gid);
      exit(1);
    }
    
    rc = trc;
  }

  return rc;
}


int
t_getgrent(int argc,
	   char *argv[],
	   void *xp,
	   unsigned long *ncp) {
  struct group *gp = NULL;
  unsigned int m = 0;
  
  
  setgrent();

  errno = 0;
  while ((gp = getgrent()) != NULL) {
    ++*ncp;
    ++m;
    if (f_verbose)
      p_group(gp);
  }

  if (errno) {
    fprintf(stderr, "%s: Error: getgrent() failed: %s\n", argv0, strerror(errno));
    exit(1);
  }

  endgrent();

  if (!m) {
    if (f_verbose)
      fprintf(stderr, "%s: Error: getgrent(): No entries found\n", argv0);
    return 1;
  }
  
  return 0;
}


int
t_getgrent_r(int argc,
	     char *argv[],
	     void *xp,
	     unsigned long *ncp) {
  char *buf = (char *) xp;
  struct group gbuf, *gp = NULL;
  int ec;
  unsigned int m = 0;
  
  
  setgrent();
  
  while ((ec = getgrent_r(&gbuf, buf, n_bufsize, &gp)) == 0 && gp) {
    ++*ncp;
    ++m;
    
    if (f_verbose)
      p_group(gp);
  }
  
  if (ec) {
    fprintf(stderr, "%s: Error: getpwent_r() failed: %s\n",
	    argv[0], strerror(ec));
    exit(1);
  }
  
  endgrent();

  if (!m) {
    if (f_verbose)
      fprintf(stderr, "%s: Error: getgrent_r(): No entries found\n", argv0);
    return 1;
  }
  
  return 0;
}


int
t_getgrouplist(int argc,
	       char *argv[],
	       void *xp,
	       unsigned long *ncp) {
  int i, k;

  
  for (i = 1; i < argc; i++) {
    struct passwd *pp = NULL;

    int ngv = n_bufsize / sizeof(gid_t);
    gid_t *gidv = (gid_t *) xp;


    pp = getpwnam(argv[i]);
    if (!pp) {
      fprintf(stderr, "%s: Error: %s: Invalid user\n", argv0, argv[i]);
      exit(1);
    }
    
    errno = 0;
    if (getgrouplist(argv[i], pp ? pp->pw_gid : -1, &gidv[0], &ngv) < 0) {
      if (errno) {
	fprintf(stderr, "%s: Error: getgrouplist(\"%s\") failed: %s\n", argv0, argv[i], strerror(errno));
	exit(1);
      }

      fprintf(stderr, "%s: Error: getgrouplist(\"%s\") failed: Buffer size too small\n", argv0, argv[i]);
      exit(1);
    }
    
    ++*ncp;
    
    if (f_verbose) {
      int first = 1;

      k = 0;
      if (!pp && ngv > 0 && gidv[0] == -1)
	++k;
	
      printf("%s:", argv[i]);
      for (; k < ngv; k++) {
	if (!first)
	  putchar(',');

	printf("%d", gidv[k]);
	first = 0;
      }
      putchar('\n');
    }
    
  }
  
  return 0;
}



static struct action {
  char *name;
  int (*tf)(int argc, char **argv, void *xp, unsigned long *ncp);
} actions[] = {
	       { "getpwnam",     &t_getpwnam },
	       { "getpwnam_r",   &t_getpwnam_r },
	       { "getpwuid",     &t_getpwuid },
	       { "getpwuid_r",   &t_getpwuid_r },
	       { "getpwent",     &t_getpwent },
	       { "getpwent_r",   &t_getpwent_r },
	       { "getgrnam",     &t_getgrnam },
	       { "getgrnam_r",   &t_getgrnam_r },
	       { "getgrgid",     &t_getgrgid },
	       { "getgrgid_r",   &t_getgrgid_r },
	       { "getgrent",     &t_getgrent },
	       { "getgrent_r",   &t_getgrent_r },
	       { "getgrouplist", &t_getgrouplist },
	       { NULL,           NULL },
};



typedef struct test_args {
  int (*tf)(int argc, char **argv, void *xp, unsigned long *ncp);
  int argc;
  char **argv;
  unsigned long nc;
  double t_min;
  double t_max;
  pthread_t tid;
} TESTARGS;



void *
run_test(void *xp) {
  TESTARGS *tap = (TESTARGS *) xp;
  char *buf = NULL;
  int j;
  struct timespec t0, t1, t2;
  
  
  buf = malloc(n_bufsize);
  if (!buf) {
    fprintf(stderr, "%s: Error: malloc(%d) failed: %s\n", argv0, n_bufsize, strerror(errno));
    exit(1);
  }

  clock_gettime(CLOCK_REALTIME, &t1);

  if (f_stayopen) {
    setpassent(1);
    setgroupent(1);
  }

  t2 = t1;
  for (j = 1; (!n_repeat || j <= n_repeat) && (!n_timeout || d_time(&t1, &t2) < n_timeout); j++) {
    int rc;
    double dt;
    
    
    rc = (*tap->tf)(tap->argc, tap->argv, (void *) buf, &tap->nc);
    if ((rc && !f_expfail) || (!rc && f_expfail)) {
      fprintf(stderr, "%s: Error: %s yielded unexpected result (rc=%d)\n", argv0, tap->argv[0], rc);
      exit(1);
    }
    
    clock_gettime(CLOCK_REALTIME, &t0);
    dt = d_time(&t2, &t0);
    
    if (!tap->t_min || dt < tap->t_min)
      tap->t_min = dt;
    if (!tap->t_max || dt > tap->t_max)
      tap->t_max = dt;
    
    t2 = t0;
  }

  free(buf);
  return NULL;
}


int
main(int argc,
     char *argv[]) {
  int  o, i, j;
  char c;
  struct timespec t1, t2;
  double dt;
  int (*tf)(int argc, char **argv, void *xp, unsigned long *ncp);
  TESTARGS *tav = NULL;
  unsigned long t_nc = 0;
  double t_min = 0;
  double t_max = 0;
  

  argv0 = argv[0];

  while ((o = getopt(argc, argv, "hvxsB:N:T:P:")) != -1)
    switch (o) {
    case 'h':
      printf("Usage:\n\t%s [<options>] <action> [<arguments>]\n", argv0);
      puts("\nOptions:");
      printf("\t-h            Display usage information\n");
      printf("\t-v            Be verbose\n");
      printf("\t-x            Expect failure\n");
      printf("\t-s            Keep database open\n");
      printf("\t-B <bytes>    Buffer size [%d bytes]\n", n_bufsize);
      printf("\t-T <seconds>  Timeout limit [%d s]\n", n_timeout);
      printf("\t-N <times>    Repeat test [%d times]\n", n_repeat);
      printf("\t-P <threads>  Run in parallel [%d threads]\n", n_threads);
      puts("\nActions:");
      for (i = 0; actions[i].name; i++)
	printf("\t%s\n", actions[i].name);
      exit(0);

    case 's':
      f_stayopen = 1;
      break;
      
    case 'v':
      ++f_verbose;
      break;
      
    case 'x':
      ++f_expfail;
      break;

    case 'B':
      c = 0;
      if (sscanf(optarg, "%u%c", &n_bufsize, &c) < 1) {
	fprintf(stderr, "%s: Error: %s: Invalid buffer size\n", argv0, optarg);
	exit(1);
      }
      
      switch (c) {
      case 0:
	break;
	
      case 'k':
      case 'K':
	n_bufsize *= 1024;
	break;
	
      case 'm':
      case 'M':
	n_bufsize *= 1024*1024;
	break;
	
      case 'g':
      case 'G':
	n_bufsize *= 1024*1024*1024;
	break;

      default:
	fprintf(stderr, "%s: Error: %s: Invalid buffer size\n", argv0, optarg);
	exit(1);
      }
      break;
      
    case 'N':
      n_repeat = atoi(optarg);
      break;

    case 'T':
      n_timeout = atoi(optarg);
      break;

    case 'P':
      n_threads = atoi(optarg);
      break;

    default:
      fprintf(stderr, "%s: Error: -%c: Invalid switch\n", argv0, c);
      exit(1);
    }

  argc -= optind;
  argv += optind;

  if (argc < 1) {
    fprintf(stderr, "%s: Error: Missing required argument: <action>\n", argv0);
    exit(1);
  }

  tf = NULL;
  for (i = 0; actions[i].name && strcmp(actions[i].name, argv[0]) != 0; i++)
    ;
  if (actions[i].name) {
    tf = actions[i].tf;
  } else {
    fprintf(stderr, "%s: Error: %s: Invalid action\n", argv0, argv[0]);
    exit(1);
  }

  t_nc = 0;
  t_min = 0.0;
  t_max = 0.0;
  
  clock_gettime(CLOCK_REALTIME, &t1);
  
  if (n_threads) {
    tav = calloc(n_threads, sizeof(*tav));
    if (!tav) {
      fprintf(stderr, "%s: Error: calloc(%d, %lu) failed: %s\n", argv0, n_threads, sizeof(*tav), strerror(errno));
      exit(1);
    }
      
    for (j = 0; j < n_threads; j++) {
      tav[j].tf = tf;
      tav[j].argc = argc;
      tav[j].argv = argv;
      tav[j].t_min = 0.0;
      tav[j].t_max = 0.0;
      tav[j].nc = 0;
      
      if (pthread_create(&tav[j].tid, NULL, run_test, (void *) &tav[j])) {
	fprintf(stderr, "%s: Error: pthread_create() failed: %s\n", argv0, strerror(errno));
	exit(1);
      }
    }

    for (j = 0; j < n_threads; j++) {
      void *res;

      pthread_join(tav[j].tid, &res);
      
      t_nc += tav[j].nc;
      if (!t_min || tav[j].t_min < t_min)
	t_min = tav[j].t_min;
      if (!t_max || tav[j].t_max > t_max)
	t_max = tav[j].t_max;
    }
  } else {
    TESTARGS ta;

    ta.tf = tf;
    ta.argc = argc;
    ta.argv = argv;
    ta.t_min = 0;
    ta.t_max = 0;
    ta.nc = 0;
    
    (void) run_test(&ta);
    
    t_nc += ta.nc;
    if (!t_min || ta.t_min < t_min)
      t_min = ta.t_min;
    if (!t_max || ta.t_max > t_max)
      t_max = ta.t_max;
  }

  clock_gettime(CLOCK_REALTIME, &t2);
  dt = d_time(&t1, &t2);

  fprintf(stderr, "[%lu call%s to %s in %s", t_nc, (t_nc == 1 ? "" : "s"), argv[0], s_time(dt));
  fprintf(stderr, " (%s/c)", s_time(dt/t_nc));
  fprintf(stderr, ", min=%s/c", s_time(t_min));
  fprintf(stderr, ", max=%s/c]\n", s_time(t_max));
  
  return 0;
}

