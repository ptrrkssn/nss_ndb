/*
 * ndb.h - NSS interface to Berkeley DB databases for passwd & group
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

#ifndef NDB_H
#define NDB_H 1

#include "config.h"

#if defined(HAVE_DB6_H)
#include <db6/db.h>
#elif defined(HAVE_DB5_H)
#include <db5/db.h>
#elif defined(HAVE_DB48_H)
#include <db48/db.h>
#else
#include <db.h>
#endif

#ifndef DB_VERSION_MAJOR
#define DB_VERSION_MAJOR 0
#define DB_NEXT R_NEXT
#define DB_NOOVERWRITE R_NOOVERWRITE
#endif

typedef struct {
  pid_t pid;
  DB *db;
#if DB_VERSION_MAJOR >= 4
  DBC *dbc;
  DB_ENV *dbe;
#endif
  char *path;
  int stayopen;
  int prev_f;
} NDB;


extern int
_ndb_open(NDB *ndb,
	  const char *path,
	  int rdwr_f);

extern void
_ndb_close(NDB *ndb);

extern int
_ndb_get(NDB *ndb,
	 DBT *key,
	 DBT *val,
	 int flags);

extern int
_ndb_put(NDB *ndb,
	 DBT *key,
	 DBT *val,
	 int flags);

extern int
_ndb_setent(NDB *ndb,
	    int stayopen,
	    const char *path);

extern int
_ndb_endent(NDB *ndb);

#endif
