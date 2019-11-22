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


#define MAXGROUPS 1024


char *argv0 = "t_libc";

char buf[8*1024*1024];

int n_repeat = 0;
int n_timeout = 2;

int f_verbose = 0;
int f_stayopen = 0;

unsigned long n = 0;


int
p_passwd(struct passwd *pp) {
  if (!pp) {
    puts("NULL");
    return -1;
  }
  
  printf("name=%s, passwd=%s, uid=%d, gid=%d, gecos=%s, home=%s, shell=%s\n",
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
  
  printf("name=%s, passwd=%s, gid=%d, members=",
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
  static char buf[256];

  if (dt < 1.0) {
    dt *= 1000.0;
    if (dt < 1.0) {
      dt *= 1000.0;
      if (dt < 1.0) {
	dt *= 1000.0;
	sprintf(buf, "%.2f ns", dt);
      } else {
	sprintf(buf, "%.2f µs", dt);
      }
    } else {
      sprintf(buf, "%.2f ms", dt);
    }
  } else {
    if (dt > 60) {
      dt /= 60.0;
      if (dt > 60) {
	dt /= 60.0;
	sprintf(buf, "%.2f h", dt);
      } else {
	sprintf(buf, "%.2f m", dt);
      }
    } else {
      sprintf(buf, "%.2f s", dt);
    }
  }
  
  return buf;
}



int
t_getpwnam(int argc,
	    char *argv[]) {
  int i;

  
  for (i = 1; i < argc; i++) {
    struct passwd *pp = NULL;
    
    
    pp = getpwnam(argv[i]);
    if (!pp) {
      fprintf(stderr, "%s: Error: getpwnam_r(\"%s\") failed: %s\n", argv0, argv[i], strerror(errno));
      exit(1);
    }
    
    ++n;
    
    if (f_verbose)
      p_passwd(pp);
  }
  
  return 0;
}

int
t_getpwnam_r(int argc,
	    char *argv[]) {
  int i;

  
  for (i = 1; i < argc; i++) {
    struct passwd pbuf, *pp = NULL;
    
    
    if (getpwnam_r(argv[i], &pbuf, buf, sizeof(buf), &pp) < 0) {
      fprintf(stderr, "%s: Error: getpwnam_r(\"%s\") failed: %s\n", argv0, argv[i], strerror(errno));
      exit(1);
    }
    
    ++n;
    
    if (f_verbose)
      p_passwd(pp);
  }

  return 0;
}

int
t_getpwent(int argc,
	   char *argv[]) {
  struct passwd *pp = NULL;

  
  setpwent();
  
  while ((pp = getpwent()) != NULL) {
    ++n;
    if (f_verbose)
      p_passwd(pp);
  }
  
  endpwent();

  return 0;
}

int
t_getpwent_r(int argc,
	     char *argv[]) {
  struct passwd pbuf, *pp = NULL;
  int rc;
  
  
  setpwent();
  
  while ((rc = getpwent_r(&pbuf, buf, sizeof(buf), &pp)) == 0 && pp) {
    ++n;
    if (f_verbose)
      p_passwd(pp);
  }
  
  if (rc < 0) {
    fprintf(stderr, "%s: Error: getpwent_r() failed: %s\n", argv0, strerror(errno));
    exit(1);
  }
  
  endpwent();
  return 0;
}



int
t_getgrnam(int argc,
	   char *argv[]) {
  int i;

  
  for (i = 1; i < argc; i++) {
    struct group *gp = NULL;
    
    
    gp = getgrnam(argv[i]);
    if (!gp) {
      fprintf(stderr, "%s: Error: getgrnam(\"%s\") failed: %s\n", argv0, argv[i], strerror(errno));
      exit(1);
    }
    
    ++n;
    
    if (f_verbose)
      p_group(gp);
  }

  return 0;
}

int
t_getgrnam_r(int argc,
	     char *argv[]) {
  int i;

  
  for (i = 1; i < argc; i++) {
    struct group gbuf, *gp = NULL;
    
    
    if (getgrnam_r(argv[i], &gbuf, buf, sizeof(buf), &gp) < 0) {
      fprintf(stderr, "%s: Error: getgrnam_r(\"%s\") failed: %s\n", argv0, argv[i], strerror(errno));
      exit(1);
    }
    
    ++n;
    
    if (f_verbose)
      p_group(gp);
  }

  return 0;
}

int
t_getgrent(int argc,
	   char *argv[]) {
  struct group *gp = NULL;
  
  
  setgrent();
  
  while ((gp = getgrent()) != NULL) {
    ++n;
    if (f_verbose)
      p_group(gp);
  }
  
  endgrent();

  return 0;
}

int
t_getgrent_r(int argc,
	     char *argv[]) {
  struct group gbuf, *gp = NULL;
  int rc;
  
  
  setgrent();
  
  while ((rc = getgrent_r(&gbuf, buf, sizeof(buf), &gp)) == 0 && gp) {
    ++n;
    if (f_verbose)
      p_group(gp);
  }
  
  if (rc < 0) {
    fprintf(stderr, "%s: Error: getpwent_r() failed: %s\n", argv[0], strerror(errno));
    exit(1);
  }
  
  endgrent();

  return 0;
}


int
t_getgrouplist(int argc,
	       char *argv[]) {
  int i, k;

  
  for (i = 1; i < argc; i++) {
    struct passwd *pp;
    
    gid_t gidv[MAXGROUPS];
    int ngv = MAXGROUPS;
    
    
    pp = getpwnam(argv[i]);
    if (!pp) {
      fprintf(stderr, "%s: Error: %s: Invalid user\n", argv0, argv[i]);
      exit(1);
    }
    
    if (getgrouplist(argv[i], pp ? pp->pw_gid : -1, &gidv[0], &ngv) < 0) {
      fprintf(stderr, "%s: Error: getgrouplist(\"%s\") failed: %s\n", argv0, argv[i], strerror(errno));
      exit(1);
    }
    
    ++n;
    
    if (f_verbose) {
      printf("%s: %d groups: \n", argv[i], ngv);
      for (k = 0; k < ngv; k++) {
	if (k > 0)
	  putchar(',');
	printf("%d", gidv[k]);
      }
      putchar('\n');
    }
    
  }
  
  return 0;
}



static struct action {
  char *name;
  int (*tf)(int argc, char **argv);
} actions[] = {
	       { "getpwnam",     &t_getpwnam },
	       { "getpwnam_r",   &t_getpwnam_r },
	       { "getpwent",     &t_getpwent },
	       { "getpwent_r",   &t_getpwent_r },
	       { "getgrnam",     &t_getgrnam },
	       { "getgrnam_r",   &t_getgrnam_r },
	       { "getgrent",     &t_getgrent },
	       { "getgrent_r",   &t_getgrent_r },
	       { "getgrouplist", &t_getgrouplist },
	       { NULL,           NULL },
};

int
main(int argc,
     char *argv[]) {
  int c, i, j;
  struct timespec t1, t2;
  double dt;
  int (*tf)(int argc, char **argv);
  

  argv0 = argv[0];
  
  while ((c = getopt(argc, argv, "hvsn:t:")) != -1)
    switch (c) {
    case 'h':
      printf("Usage:\n\t%s [<options>] <action> [<arguments>]\n", argv0);
      puts("\nOptions:");
      printf("\t-h            Display usage information\n");
      printf("\t-v            Be verbose\n");
      printf("\t-s            Keep database open\n");
      printf("\t-n <times>    Repeat test [%d times]\n", n_repeat);
      printf("\t-t <seconds>  Timeout limit [%d s]\n", n_timeout);
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
      
    case 'n':
      n_repeat = atoi(optarg);
      break;

    case 't':
      n_timeout = atoi(optarg);
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
  if (actions[i].name)
    tf = actions[i].tf;
  else {
    fprintf(stderr, "%s: Error: %s: Invalid action\n", argv0, argv[0]);
    exit(1);
  }
  
  clock_gettime(CLOCK_REALTIME, &t1);

  if (f_stayopen) {
    setpassent(1);
    setgroupent(1);
  }

  t2 = t1;
  for (j = 1; (!n_repeat || j <= n_repeat) && (!n_timeout || d_time(&t1, &t2) < n_timeout); j++) {
    if ((*tf)(argc, argv))
      break;

    if (n_timeout)
      clock_gettime(CLOCK_REALTIME, &t2);
  }
  
  clock_gettime(CLOCK_REALTIME, &t2);
  dt = d_time(&t1, &t2);

  printf("%lu entries retrieved in %s", n, s_time(dt));
  printf(" (%s per entry)\n", s_time(dt/n));
  
  return 0;
}


