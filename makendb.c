/*
 * makendb.c - Generate Berkeley DB databases for passwd & group
 *
 * Copyright (c) 2017 Peter Eriksson <pen@lysator.liu.se>
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
#include <stdarg.h>
#include <db.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <ctype.h>


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


int
main(int argc,
     char *argv[]) {
  DB *db_id = NULL, *db_name = NULL, *db = NULL;
  char buf[2048];
  DBT key, val;
  int rc, ni, line;
  char *name, *id, *cp;
  char *type = NULL;
  char path[2048], *p_name, *p_id;
  int i, j;
  int print_f = 0;
  int unique_f = 0;
  int verbose_f = 0;
  int key_f = 0;
  char *delim = ":";
  int nw = 0;
  
  
  for (i = 1; i < argc && argv[i][0] == '-'; i++) {
    for (j = 1; argv[i][j]; j++) {
      switch (argv[i][j]) {
      case 'v':
	++verbose_f;
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
	printf("Usage: %s [-h] [-u] [-p] [-k] [-T passwd|group] [-D <delim>] <db-path>\n", argv[0]);
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
      strcpy(path, argv[i]);
      db = dbopen(path, O_RDONLY, 0, DB_BTREE, NULL);
      if (!db) {
	sprintf(path, "%s.db", argv[i]);
	db = dbopen(path, O_RDONLY, 0, DB_BTREE, NULL);
      }
      if (!db) {
	sprintf(path, "%s.byname.db", argv[i]);
	db = dbopen(path, O_RDONLY, 0, DB_BTREE, NULL);
      }
      if (!db) {
	fprintf(stderr, "%s: %s: dbopen: %s\n",
		argv[0], argv[i], strerror(errno));
	exit(1);
      }
      p_name = strdup(path);
      
      do {
	key.data = NULL;
	key.size = 0;

	val.data = buf;
	val.size = sizeof(buf);
	
	rc = db->seq(db, &key, &val, R_NEXT);
	if (rc == 0) {
	  if (key_f)
	    printf("%-14.*s\t", (int) key.size, key.data);
	  printf("%.*s\n", (int) val.size, val.data);
	}
      } while (rc == 0);
      
      db->close(db);
    }
    return 0;
  }
  
  if (type == NULL) {

    sprintf(path, "%s", argv[i]);
    db_name = dbopen(path, O_RDWR|O_CREAT, 0644, DB_BTREE, NULL);
    if (!db_name) {
      sprintf(path, "%s.db", argv[i]);
      db_name = dbopen(path, O_RDWR|O_CREAT, 0644, DB_BTREE, NULL);
    }
    if (!db_name) {
      fprintf(stderr, "%s: %s: dbopen: %s\n", argv[0], path, strerror(errno));
      exit(1);
    }
    p_name = strdup(path);
    
  } else if (strcmp(type, "passwd") == 0) {
    
    sprintf(path, "%s.byuid.db", argv[i]);
    db_id = dbopen(path, O_RDWR|O_CREAT, 0644, DB_BTREE, NULL);
    if (!db_id) {
      fprintf(stderr, "%s: %s: dbopen: %s\n", argv[0], path, strerror(errno));
      exit(1);
    }
    p_id = strdup(path);
    
    sprintf(path, "%s.byname.db", argv[i]);
    db_name = dbopen(path, O_RDWR|O_CREAT, 0644, DB_BTREE, NULL);
    if (!db_id) {
      fprintf(stderr, "%s: %s: dbopen: %s\n", argv[0], path, strerror(errno));
      exit(1);
    }
    p_name = strdup(path);
    
  } else if (strcmp(type, "group") == 0) {
    
    sprintf(path, "%s.bygid.db", argv[i]);
    db_id = dbopen(path, O_RDWR|O_CREAT, 0644, DB_BTREE, NULL);
    if (!db_id) {
      fprintf(stderr, "%s: %s: dbopen: %s\n", argv[0], path, strerror(errno));
      exit(1);
    }
    p_id = strdup(path);
    
    sprintf(path, "%s.byname.db", argv[i]);
    db_name = dbopen(path, O_RDWR|O_CREAT, 0644, DB_BTREE, NULL);
    if (!db_id) {
      fprintf(stderr, "%s: %s: dbopen: %s\n", argv[0], path, strerror(errno));
      exit(1);
    }
    p_name = strdup(path);
    
  } else {
  
    fprintf(stderr, "%s: %s: Invalid DB type\n", argv[0], type);
    exit(1);
    
  }

  ni = 0;
  line = 0;
  
  while (fgets(buf, sizeof(buf), stdin)) {
    char *ptr = NULL;

    ++line;
    trim(buf);
    if (*buf == '#' || !*buf)
      continue;

    val.data = strdup(buf);
    val.size = strlen(buf)+1;

    ptr = buf;
    name = strsep(&ptr, delim);

    if (db_id) {
      (void) strsep(&ptr, delim); /* ignore pass */
      id = strsep(&ptr, delim);

      key.data = id;
      key.size = strlen(id);
      
      rc = db_id->put(db_id, &key, &val, unique_f ? R_NOOVERWRITE : 0);
      if (rc < 0) {
	fprintf(stderr, "%s: %s: %s: db->put: %s\n", argv[0], p_id, id, strerror(errno));
	exit(1);
      } else if (rc > 0) {
	fprintf(stderr, "%s: %s: %s: Key already exists in database\n", argv[0], p_id, id);
	nw++;
      }
    }
    
    key.data = name;
    key.size = strlen(name);

    rc = db_name->put(db_name, &key, &val, unique_f ? R_NOOVERWRITE : 0);
    if (rc < 0) {
      fprintf(stderr, "%s: %s: %s: db->put: %s\n", argv[0], p_name, name, strerror(errno));
      exit(1);
    } else if (rc > 0) {
      fprintf(stderr, "%s: %s: %s: Key already exists in database\n", argv[0], p_name, name);
      nw++;
    }

    ++ni;
  }

  db_name->close(db_name);
  
  if (db_id)
    db_id->close(db_id);

  if (verbose_f)
    fprintf(stderr, "%u entries imported (%u warning%s)\n", ni, nw, nw == 1 ? "" : "s");
  
  return 0;
}
