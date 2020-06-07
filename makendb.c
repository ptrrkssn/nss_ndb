/*
 * makendb.c - Generate Berkeley DB databases for passwd & group
 *
 * Copyright (c) 2017-2019 Peter Eriksson <pen@lysator.liu.se>
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <db.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "nss_ndb.h"
#include "ndb.h"

int debug_f = 0;
int print_f = 0;
int unique_f = 0;
int verbose_f = 0;
int key_f = 0;

char *
trim(char *buf) {
  char *cp;

  
  if (!buf)
    return NULL;
  
  for (cp = buf; isspace(*cp); cp++)
    ;

  if (cp > buf)
    strcpy(buf, cp);

  for (cp = buf+strlen(buf)-1; cp >= buf && isspace(*cp); cp--)
    *cp = '\0';

  return buf;
}


char *
strxdup(const char *str) {
  if (!str)
    return NULL;
  return strdup(str);
}


void
version(FILE *fp) {
  fprintf(fp,
	  "[makendb, version %s - Copyright (c) 2017-2019 Peter Eriksson <pen@lysator.liu.se>]\n",
	  PACKAGE_VERSION);
}


int
add_user_group(NDB *db,
	       char *gid,
	       char *members) {
  DBT key, val;
  char *cp;
  int rc;

  
  if (debug_f)
    fprintf(stderr, "* add_user_group(%s, %s): \n", gid, members);
  
  while ((cp = strsep(&members, ",")) != NULL) {
    if (debug_f)
      fprintf(stderr, "** looking up user: %s\n", cp);

    key.data = cp;
    key.size = strlen(cp);

    val.data = NULL;
    val.size = 0;

    rc = _ndb_get(db, &key, &val, 0);
    if (rc == 0) {
      /* Old record - append */
      int found;
      char *grp, *grplist = val.data;
      char *buf = malloc(val.size + 64);
      if (!buf)
	return -1;
      
      strcpy(buf, val.data);
      
      if (debug_f)
	fprintf(stderr, "*** add_user_group: %s: Old Record: %s\n", cp, grplist);

      (void) strsep(&grplist, ":");
      found = 0;
      while ((grp = strsep(&grplist, ",")) != NULL) {
	if (debug_f)
	  fprintf(stderr, "***** add_user_group: %s: Checking %s vs %s\n", cp, grp, gid);
	
	if (strcmp(grp, gid) == 0) {
	  if (debug_f)
	    fprintf(stderr, "**** add_user_group: %s: GID already on list: %s\n", cp, gid);
	  found = 1;
	  break;
	}
      }

      if (!found) {
	if (debug_f)
	  fprintf(stderr, "**** add_user_group: %s: Adding new gid: %s\n", cp, gid);
	
	strcat(buf, ",");
	strcat(buf, gid);

	memset(&val, 0, sizeof(val));
	val.data = buf;
	val.size = strlen(buf)+1;
	
	rc = _ndb_put(db, &key, &val, 0);
	if (rc < 0) {
	  if (debug_f)
	    fprintf(stderr, "*** add_user_group: %.*s: db->put: %s\n",
		    (int) key.size, (char *) key.data, strerror(errno));
	  return -1;
	}
	
	free(buf);
      }
      
    } else if (rc == 1) {
      /* New record - create*/
      char buf[256];
      int rc;
      
      
      if (debug_f)
	fprintf(stderr, "*** add_user_group: %s: New Record\n", cp);
      
      rc = snprintf(buf, sizeof(buf), "%s:%s", cp, gid);
      if (rc < 0) {
	fprintf(stderr, "*** add_user_group: snprintf(): %s\n", strerror(errno));
	return -1;
      }
      if (rc >= sizeof(buf)) {
	fprintf(stderr, "*** add_user_group: snprintf(): buf too small\n");
	return -1;
      }
      
      memset(&val, 0, sizeof(val));
      val.data = buf;
      val.size = strlen(buf)+1;
      
      rc = _ndb_put(db, &key, &val, 0);
      if (rc < 0) {
	if (debug_f)
	  fprintf(stderr, "*** add_user_group: %.*s: db->put: %s\n",
		  (int) key.size, (char *) key.data, strerror(errno));
	return -1;
      }
      
    }
  }

  return 0;
}


int
main(int argc,
     char *argv[]) {
  NDB db_id, db_name, db_user, db;
  DBT key, val;
  int rc, ni,line, fd;
  char *name, *cp, *buf;
  char *id = NULL;
  char *type = NULL;
  char path[2048], *p_name, *p_id, *p_user;
  int i, j;
  char *delim = ":";
  int nw = 0;
  ssize_t len;
  struct stat sb;
  
  memset(&db_id, 0, sizeof(db_id));
  memset(&db_name, 0, sizeof(db_name));
  memset(&db_user, 0, sizeof(db_user));
  memset(&db, 0, sizeof(db));
  
  for (i = 1; i < argc && argv[i][0] == '-'; i++) {
    for (j = 1; argv[i][j]; j++) {
      switch (argv[i][j]) {
      case 'V':
	version(stdout);
	exit(0);
	  
      case 'v':
	++verbose_f;
	version(stderr);
	break;
	
      case 'd':
	++debug_f;
	break;
	
      case 'k':
	++key_f;
	break;
	
      case 'p':
	++print_f;
	break;
	
      case 'u':
	++unique_f;
	break;
	
      case 'D':
	if (argv[i][j+1])
	  cp = argv[i]+j+1;
	else if (i+1 < argc)
	  cp = argv[++i];
	else
	  cp = NULL;
	delim = strxdup(cp);
	goto NextArg;
	
      case 'T':
	if (argv[i][j+1])
	  cp = argv[i]+j+1;
	else if (i+1 < argc)
	  cp = argv[++i];
	else
	  cp = NULL;
	type = strxdup(cp);
	goto NextArg;
	
      case 'h':
	printf("Usage: %s [-h] [-V] [-v] [-u] [-p] [-k] [-T passwd|group] [-D <delim>] <db-path> <src-file>\n", argv[0]);
	exit(0);
	
      default:
	fprintf(stderr, "%s: %s: Invalid switch\n", argv[0], argv[i]);
	exit(1);
      }
    }
  NextArg:;
  }
  
  if (i >= argc) {
    fprintf(stderr, "%s: Missing required database path\n", argv[0]);
    exit(1);
  }

  if (print_f) {

    for (; i < argc; i++) {
      int rc;

      strcpy(path, argv[i]);
      rc = _ndb_open(&db, path, 0);
      if (rc < 0) {
	sprintf(path, "%s.db", argv[i]);
	rc = _ndb_open(&db, path, 0);
      }
      if (rc < 0) {
	sprintf(path, "%s.byname.db", argv[i]);
	rc = _ndb_open(&db, path, 0);
      }
      if (rc < 0) {
	fprintf(stderr, "%s: %s: dbopen: %s\n",
		argv[0], argv[i], strerror(errno));
	exit(1);
      }

      p_name = strdup(path);

      _ndb_setent(&db, 1, path);

      do {
	key.data = NULL;
	key.size = 0;

	val.data = NULL;
	val.size = 0;

	rc = _ndb_get(&db, &key, &val, DB_NEXT);
	if (rc == 0) {
	  if (key_f)
	    printf("%-14.*s\t", (int) key.size, (char *) key.data);
	  printf("%.*s\n", (int) val.size, (char *) val.data);
	}
      } while (rc == 0);

      _ndb_close(&db);
    }
    return 0;
  }

  p_id = p_name = p_user = NULL;
    
  if (type == NULL) {

    sprintf(path, "%s", argv[i]);
    rc = _ndb_open(&db_name, path, 1);
    if (rc < 0) {
      sprintf(path, "%s.db", argv[i]);
      rc = _ndb_open(&db_name, path, 1);
    }
    if (rc < 0) {
      fprintf(stderr, "%s: %s: dbopen: %s\n", argv[0], path, strerror(errno));
      exit(1);
    }
    p_name = strdup(path);
    
  } else if (strcmp(type, "passwd") == 0) {
    
    sprintf(path, "%s/passwd.byuid.db", argv[i]);
    rc = _ndb_open(&db_id, path, 1);
    if (rc < 0) {
      fprintf(stderr, "%s: %s: dbopen: %s\n", argv[0], path, strerror(errno));
      exit(1);
    }
    p_id = strdup(path);
    
    sprintf(path, "%s/passwd.byname.db", argv[i]);
    rc = _ndb_open(&db_name, path, 1);
    if (rc < 0) {
      fprintf(stderr, "%s: %s: dbopen: %s\n", argv[0], path, strerror(errno));
      exit(1);
    }
    p_name = strdup(path);
    
    sprintf(path, "%s/group.byuser.db", argv[i]);
    rc = _ndb_open(&db_user, path, 1);
    if (rc < 0) {
      fprintf(stderr, "%s: %s: dbopen: %s\n", argv[0], path, strerror(errno));
      exit(1);
    }
    p_user = strdup(path);
    
  } else if (strcmp(type, "group") == 0) {
    
    sprintf(path, "%s/group.bygid.db", argv[i]);
    rc = _ndb_open(&db_id, path, 1);
    if (rc < 0) {
      fprintf(stderr, "%s: %s: dbopen: %s\n", argv[0], path, strerror(errno));
      exit(1);
    }
    p_id = strdup(path);
    
    sprintf(path, "%s/group.byname.db", argv[i]);
    rc = _ndb_open(&db_name, path, 1);
    if (rc < 0) {
      fprintf(stderr, "%s: %s: dbopen: %s\n", argv[0], path, strerror(errno));
      exit(1);
    }
    p_name = strdup(path);
    
    sprintf(path, "%s/group.byuser.db", argv[i]);
    rc = _ndb_open(&db_user, path, 1);
    if (rc < 0) {
      fprintf(stderr, "%s: %s: dbopen: %s\n", argv[0], path, strerror(errno));
      exit(1);
    }
    p_user = strdup(path);
    
  } else {
  
    fprintf(stderr, "%s: %s: Invalid DB type\n", argv[0], type);
    exit(1);
    
  }

  ni = 0;
  line = 0;
  
  ++i;
  if (i >= argc) {
    fprintf(stderr, "%s: Missing required source file\n", argv[0]);
    exit(1);
  }

  fd = open(argv[i], O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "%s: Error: %s: open failed: %s\n", argv[0], argv[i], strerror(errno));
    exit(1);
  }
  
  if (fstat(fd, &sb) < 0) {
    fprintf(stderr, "%s: Error: %s: fstat failed: %s\n", argv[0], argv[i], strerror(errno));
    exit(1);
  }
  
  buf = malloc(sb.st_size+1);
  if (!buf) {
    fprintf(stderr, "%s: Error: malloc(%lu bytes) failed: %s\n", argv[0], sb.st_size+1, strerror(errno));
    exit(1);
  }
  
  len = read(fd, buf, sb.st_size);
  if (len < 0) {
    fprintf(stderr, "%s: Error: read(%lu bytes) failed: %s\n", argv[0], sb.st_size, strerror(errno));
    exit(1);
  }
  
  buf[len] = 0;
  close(fd);

  cp = buf;
  while ((buf = cp) && *buf) {
    char *ptr = NULL;
    
    cp = strchr(buf, '\n');
    if (cp)
      *cp++ = 0;
    else
      cp = buf+strlen(buf);

    ++line;
    trim(buf);
    if (*buf == '#' || !*buf)
      continue;

    if (debug_f)
      printf("[%s]\n", buf);
    
    val.data = strdup(buf);
    val.size = strlen(buf)+1;

    ptr = buf;
    name = strsep(&ptr, delim);

    if (db_id.db) {
      (void) strsep(&ptr, delim); /* ignore pass */
      id = strsep(&ptr, delim);

      if (id) {
	key.data = id;
	key.size = strlen(id);
	
	rc = _ndb_put(&db_id, &key, &val, unique_f ? DB_NOOVERWRITE : 0);
	if (rc < 0) {
	  fprintf(stderr, "%s: %s: %s: db->put: %s\n", argv[0], p_id, id, strerror(errno));
	  exit(1);
	} else if (rc > 0) {
	  fprintf(stderr, "%s: %s: %s: Key already exists in database\n", argv[0], p_id, id);
	  nw++;
	}
      }
    }
    
    key.data = name;
    key.size = strlen(name);

    rc = _ndb_put(&db_name, &key, &val, unique_f ? DB_NOOVERWRITE : 0);
    if (rc < 0) {
      fprintf(stderr, "%s: %s: %s: db->put: %s\n", argv[0], p_name, name, strerror(errno));
      exit(1);
    } else if (rc > 0) {
      fprintf(stderr, "%s: %s: %s: Key already exists in database\n", argv[0], p_name, name);
      nw++;
    }

    if (db_user.db && id && type) {
      if (strcmp(type, "group") == 0 && ptr && *ptr) {
	add_user_group(&db_user, id, ptr);
      } else {
	add_user_group(&db_user, id, name);
      }
    }
    
    ++ni;
  }

  _ndb_close(&db_name);
  _ndb_close(&db_id);
  _ndb_close(&db_user);

  if (verbose_f)
    fprintf(stderr, "%u entries imported (%u warning%s)\n", ni, nw, nw == 1 ? "" : "s");

  return 0;
}
