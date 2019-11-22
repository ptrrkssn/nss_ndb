/*
 * nss_ndb.h - NSS interface to Berkeley DB databases for passwd & group
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

#ifndef NSS_NDB_H
#define NSS_NDB_H 1

#include <unistd.h>
#include <stdarg.h>
#include <nsswitch.h>


/* master.passwd has 10 fields, standard passwd has 7 + extra NULL */
#ifdef _PATH_MASTERPASSWD
#define MAXPWFIELDS 11
#else
#define MAXPWFIELDS 8
#endif

#define MAXGRFIELDS 5

#ifndef PATH_NSS_NDB
#define PATH_NSS_NDB "/var/db/nss_ndb"
#endif

#define PATH_NSS_NDB_PASSWD_BY_UID       PATH_NSS_NDB "/passwd.byuid.db"
#define PATH_NSS_NDB_PASSWD_BY_NAME      PATH_NSS_NDB "/passwd.byname.db"
#define PATH_NSS_NDB_GROUP_BY_GID        PATH_NSS_NDB "/group.bygid.db"
#define PATH_NSS_NDB_GROUP_BY_NAME       PATH_NSS_NDB "/group.byname.db"
#define PATH_NSS_NDB_USERGROUPS_BY_NAME  PATH_NSS_NDB "/group.byuser.db"

typedef int (*STR2OBJ)(char *str,
		       size_t len,
		       void **vp,
		       char **buf,
		       size_t *blen,
		       size_t maxsize);

enum setent_constants {
  SETENT = 1,
  ENDENT = 2
};


extern int
nss_ndb_getpwnam_r(void *rv,
		   void *mdata,
		   va_list ap);

extern int
nss_ndb_getpwuid_r(void *rv,
		   void *mdata,
		   va_list ap);

extern int
nss_ndb_getpwent_r(void *rv,
		   void *mdata,
		   va_list ap);

extern int
nss_ndb_setpwent(void *rv,
		 void *mdata,
		 va_list ap);

extern int
nss_ndb_endpwent(void *rv,
		 void *mdata,
		 va_list ap);

extern int
nss_ndb_getgrnam_r(void *rv,
		   void *mdata,
		   va_list ap);

extern int
nss_ndb_getgrgid_r(void *rv,
		   void *mdata,
		   va_list ap);

extern int
nss_ndb_getgrent_r(void *rv,
		   void *mdata,
		   va_list ap);

extern int
nss_ndb_setgrent(void *rv,
		 void *mdata,
		 va_list ap);

extern int
nss_ndb_endgrent(void *rv,
		 void *mdata,
		 va_list ap);

extern int
nss_ndb_getgroupmembership(void *res,
			   void *mdata,
			   va_list ap);

extern ns_mtab *
nss_module_register(const char *modname,
		    unsigned int *plen,
		    nss_module_unregister_fn *fptr);


#endif
