/*
 * nss_ndb.c - NSS interface to Berkeley DB databases for passwd & group
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
#include <fcntl.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <db.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>

/* #include <stdarg.h> */
/* #include <nsswitch.h> */

#include "nss_ndb.h"

#define MAX_GETENT_SIZE 1024
#define MAX_GETOBJ_SIZE 32768

static char *path_passwd_byname     = PATH_NSS_NDB_PASSWD_BY_NAME;
static char *path_passwd_byuid      = PATH_NSS_NDB_PASSWD_BY_UID;
static char *path_group_byname      = PATH_NSS_NDB_GROUP_BY_NAME;
static char *path_group_bygid       = PATH_NSS_NDB_GROUP_BY_GID;
static char *path_usergroups_byname = PATH_NSS_NDB_USERGROUPS_BY_NAME;

typedef int (*STR2OBJ)(char *str, size_t len, void **vp, char **buf, size_t *blen, size_t maxsize);


static __thread DB  *db_getpwent = NULL;
static __thread DB  *db_getgrent = NULL;

#if BTREEVERSION >= 9
static __thread DBC *dbc_getpwent = NULL;
static __thread DBC *dbc_getgrent = NULL;
#endif



static int
ndb_get(DB *db,
	DBT *key,
	DBT *val,
	int flags) {
  if (!db)
    return -1;

  /* XXX: key.flags = DB_DBT_USERMEM; */

  return db->get(db,
#if BTREEVERSION >= 9
		 NULL,
#endif
		 key, val, flags);
  
}

static void
ndb_close(DB *db) {
  if (!db)
    return;

  db->close(db
#if BTREEVERSION >= 9
	    , 0
#endif
	    );
}

static DB *
ndb_open(const char *path) {
  DB *db;
#if BTREEVERSION >= 9
  int ret;
#endif

#if DEBUG
  fprintf(stderr, "*** ndb_open(%s)\n", path);
#endif

#if BTREEVERSION >= 9
  ret = db_create(&db, NULL, 0);
  if (ret) {
#if DEBUG
    fprintf(stderr, "\t -> db_create()  returned %d\n", ret);
#endif
    return NULL;
  }

  ret = db->open(db, NULL, path, NULL, DB_UNKNOWN, DB_RDONLY, 0);
  if (ret) {
#if DEBUG
    fprintf(stderr, "\t -> db->open()  returned %d\n", ret);
#endif
    db->close(db, 0);
    return NULL;
  }
#else
  db = dbopen(path, O_RDONLY, 0, DB_BTREE, NULL);
#endif
  
#if DEBUG
  fprintf(stderr, "\t -> db = %p\n", db);
#endif
  return db;
}




static void *
balloc(size_t size,
       char **buf,
       size_t *blen) {
  if (size > *blen) {
#if DEBUG
    fprintf(stderr, "*** balloc(%lu) failed (available: %lu)\n", size, *blen);
#endif
    errno = ERANGE;
    return NULL;
  }

  void *rptr = *buf;
  *buf += size;
  *blen -= size;

  return rptr;
}


static int
strsplit(char *buf,
	 int sep,
	 char *fv[],
	 int fs) {
  int n = 0;
  char *cp;

  
  if (!buf)
    return 0;
  
  while ((cp = strchr(buf, sep)) != NULL) {
    if (n >= fs) {
      errno = EOVERFLOW;
      return -1;
    }
    
    fv[n++] = buf;
    *cp++ = '\0';
    buf = cp;
    
  }
  
  if (n >= fs) {
    errno = EOVERFLOW;
    return -1;
  }
    
  fv[n++] = buf;
  if (n < fs)
    fv[n] = NULL;

  return n;
}


static char *
strbdup(const char *str,
	char **buf,
	size_t *blen) {
  int len;
  char *rstr;
  

  if (!str)
    return NULL;
  
  len = strlen(str)+1;
  if (len > *blen) {
    errno = ERANGE;
    return NULL;
  }

  memcpy(rstr = *buf, str, len);
  *blen -= len;
  *buf  += len;

  return rstr;
}

/* -------------------------------------------------- */

static int
str2passwd(char *str,
	   size_t size,
	   struct passwd *pp,
	   char **buf,
	   size_t *blen,
	   size_t maxsize) {
  char *btmp;
  char *fv[MAXPWFIELDS];
  int fc, rc, i;

  
  if (!str || !pp) {
    errno = EINVAL;
    return -1;
  }

#if DEBUG 
  fprintf(stderr, "str2passwd(\"%.*s\", blen=%lu)\n", (int) size, str, *blen);
#endif

  memset(pp, 0, sizeof(*pp));

  btmp = strndup(str, size);
  if (!btmp) {
    return -1;
  }

  fc = strsplit(btmp, ':', fv, MAXPWFIELDS);
  if (fc != 7
#ifdef _PATH_MASTERPASSWD
      && fc != 10
#endif
      ) {
    goto Fail;
  }
  
  pp->pw_name = strbdup(fv[0], buf, blen);
  if (!pp->pw_name) {
    goto Fail;
  }
  
  pp->pw_passwd = strbdup(fv[1], buf, blen);
  if (!pp->pw_passwd) {
    goto Fail;
  }
  
  if ((rc = sscanf(fv[2], "%u", &pp->pw_uid)) != 1) {
    errno = EINVAL;
    goto Fail;
  }
  
  if ((rc = sscanf(fv[3], "%u", &pp->pw_gid)) != 1) {
    errno = EINVAL;
    goto Fail;
  }

  i = 3;
  
#ifdef _PATH_MASTERPASSWD
  if (fc == 10) {
    pp->pw_class = strbdup(fv[++i], buf, blen);
    if (!pp->pw_class) {
      goto Fail;
    }
  
    if ((rc = sscanf(fv[++i], "%lu", &pp->pw_change)) != 1) {
      errno = EINVAL;
      goto Fail;
    }
  
    if ((rc = sscanf(fv[++i], "%lu", &pp->pw_expire)) != 1) {
      errno = EINVAL;
      goto Fail;
    }
  } else {
    pp->pw_class = "";
    pp->pw_change = 0;
    pp->pw_expire = 0;
  }
#endif
  
  pp->pw_gecos = strbdup(fv[++i], buf, blen);
  if (!pp->pw_gecos) {
    goto Fail;
  }
  
  pp->pw_dir = strbdup(fv[++i], buf, blen);
  if (!pp->pw_dir) {
    goto Fail;
  }
  
  pp->pw_shell = strbdup(fv[++i], buf, blen);
  if (!pp->pw_shell) {
    goto Fail;
  }
  
  free(btmp);
  pp->pw_fields = fc;
  return 0;

 Fail:
  free(btmp);
  return -1;
}



static int
str2group(char *str,
	  size_t size,
	  struct group *gp,
	  char **buf,
	  size_t *blen,
	  size_t maxsize) {
  char *tmp, *members, *btmp;
  int ng = 0;
  char *fv[MAXGRFIELDS];
  int fc, rc, i;


  if (!str || !gp) {
    errno = EINVAL;
    return -1;
  }

#if DEBUG
  fprintf(stderr, "str2group(\"%.*s\", size=%lu, blen=%lu, maxsize=%lu)\n", (int) size, str, size, *blen, maxsize);
#endif
  
  memset(gp, 0, sizeof(*gp));
  
  btmp = strndup(str, size);
  if (!btmp)
    return -1;
  
  fc = strsplit(btmp, ':', fv, MAXGRFIELDS);
  if (fc != 4)
    goto Fail;
  
  gp->gr_name = strbdup(fv[0], buf, blen);
  if (!gp->gr_name)
    goto Fail;
  
  gp->gr_passwd = strbdup(fv[1], buf, blen);
  if (!gp->gr_passwd)
    goto Fail;
  
  if ((rc = sscanf(fv[2], "%u", &gp->gr_gid)) != 1) {
    errno = EINVAL;
    goto Fail;
  }

  if (size < maxsize) {
    members = fv[3];
    if (members) {
      char *cp;
      
      ++ng;
      for (cp = members; *cp; cp++)
	if (*cp == ',')
	  ++ng;
    }
    
    gp->gr_mem = balloc((ng+1)*sizeof(char *), buf, blen);
    if (!gp->gr_mem) {
      goto Fail;
    }
    
    i = 0;
    if (members) {
      for (; (tmp = strsep(&members, ",")) != NULL; i++) {
	gp->gr_mem[i] = strbdup(tmp, buf, blen);
	if (!gp->gr_mem[i]) {
	  goto Fail;
	}
      }
    }
  } else {
    /* list of group members too big, give empty list */
    
    gp->gr_mem = balloc(2*sizeof(char *), buf, blen);
    if (!gp->gr_mem) {
      goto Fail;
    }

    gp->gr_mem[0] = strbdup("E$OVERSIZED-GROUP-USER-LIST", buf, blen);
    if (!gp->gr_mem[0]) {
      goto Fail;
    }
    i = 1;
  }
  
  gp->gr_mem[i] = NULL;
  return 0;

 Fail:
  free(btmp);
  return -1;
}

/* ------------------------------------------------------------ */

static int
ndb_getkey_r(DB *db_getxxent,
	     const char *path,
	     STR2OBJ str2obj,
	     void *rv,
	     void *mdata,
	     char *name,
	     void *pbuf,
	     char *buf,
	     size_t bsize,
	     int *res) {
  void **ptr = rv;

  DB *db;
  DBT key, val;
  int rc;
  
  if (db_getxxent)
    db = db_getxxent;
  else
    db = ndb_open(path);
  
  if (!db)
    return NS_UNAVAIL;
  
  *ptr = 0;
  
  memset(&key, 0, sizeof(key));
  memset(&val, 0, sizeof(val));
  
  key.data = name;
  key.size = strlen(name);
  
  rc = ndb_get(db, &key, &val, 0);
  
  if (rc < 0) {
    *res = errno;
    if (!db_getxxent)
      ndb_close(db);
    return NS_UNAVAIL;
  }
  else if (rc > 0) {
    if (!db_getxxent)
      ndb_close(db);
    return NS_NOTFOUND;
  }

  if (!db_getxxent)
    ndb_close(db);
  
  if ((*str2obj)(val.data, val.size, pbuf, &buf, &bsize, MAX_GETOBJ_SIZE) < 0) {
    *res = errno;

#if DEBUG
    if (errno == ERANGE)
      fprintf(stderr, "ndb_getkey_r: val.data=\"%.10s...\", val.size=%lu, bsize=%lu\n",
	      val.data, val.size, bsize);
#endif
    
    return NS_NOTFOUND;
  }

  *ptr = pbuf;
  return NS_SUCCESS;
}
  

static int
ndb_getent_r(DB **dbp,
#if BTREEVERSION >= 9
	     DBC **dbcp,
#endif
	     const char *dbpath,
	     STR2OBJ str2obj,
	     void *rv,
	     void *mdata,
	     void *pbuf,
	     char *buf,
	     size_t bsize,
	     int *res) {
  void **ptr = rv;
  
  int rc;
  DBT key, val;
  

  if (!dbp
#if BTREEVERSION >= 9
      || !dbcp
#endif
      )
    return NS_UNAVAIL;
  
  if (!*dbp)
    *dbp = ndb_open(dbpath);
  if (!*dbp)
    return NS_UNAVAIL;

#if BTREEVERSION >= 9
  if (!*dbcp) {
    if ((rc = (*dbp)->cursor(*dbp, NULL, dbcp, 0)) || !*dbcp) {
#if DEBUG
      fprintf(stderr, "\t -> cursor failed with rc=%d\n", rc);
#endif
      return NS_UNAVAIL;
    }
  }
#endif
  
  *ptr = 0;

  memset(&key, 0, sizeof(key));
  memset(&val, 0, sizeof(val));

#if BTREEVERSION >= 9
  rc = (*dbcp)->get(*dbcp, &key, &val, DB_NEXT);
#else
  rc = (*dbp)->seq(*dbp, &key, &val, R_NEXT);
#endif
#if DEBUG
  fprintf(stderr, "\t -> get() returned rc=%d\n", rc);
#endif
  
  *res = errno;
  
  if (rc < 0) {
#if BTREEVERSION >= 9
    (*dbcp)->close(*dbcp);
    *dbcp = NULL;
#endif
    
    ndb_close(*dbp);
    *dbp = NULL;
    
    return NS_UNAVAIL;
  }
  else if (rc > 0) {
    return NS_NOTFOUND;
  }

  if ((*str2obj)(val.data, val.size, pbuf, &buf, &bsize, MAX_GETENT_SIZE) < 0) {
   *res = errno;

#if DEBUG
    if (errno == ERANGE)
      fprintf(stderr, "ndb_getent_r: val.data=\"%.10s...\", errno==ERANGE, val.size=%lu, bsize=%lu\n",
	      val.data, val.size, bsize);
#endif
    
    /* XXX: If errno == ERANGE, go backwards one db record */
    return NS_NOTFOUND;
  }

  *ptr = pbuf;
  return NS_SUCCESS;
}


static int
ndb_setent(DB **dbp,
#if BTREEVERSION >= 9
	   DBC **dbcp,
#endif
	   int stayopen,
	   const char *path,
	   void *rv,
	   void *mdata) {
#if BTREEVERSION >= 9
  if (*dbcp) {
    (*dbcp)->close(*dbcp);
    *dbcp = NULL;
  }
#endif
  
  if ((enum setent_constants) mdata == SETENT && stayopen)
    return NS_SUCCESS;
  
  if (*dbp) {
    ndb_close(*dbp);
    *dbp = NULL;
  }
  
  if ((enum setent_constants) mdata == SETENT) {
    *dbp = ndb_open(path);
    if (!*dbp)
      return NS_UNAVAIL;
  }
  
  return NS_SUCCESS;
}



static int
ndb_endent(DB **dbp,
#if BTREEVERSION >= 9
	   DBC **dbcp,
#endif
	   void *rv,
	   void *mdata,
	   int *res) {
  
#if BTREEVERSION >= 9
  if (*dbcp) {
    (*dbcp)->close(*dbcp);
    *dbcp = NULL;
  }
#endif
  
  if (*dbp) {
    ndb_close(*dbp);
    *dbp = NULL;
  }
  
  return NS_SUCCESS;
}


/* ---------------------------------------------------------------------- */

int
nss_ndb_getpwnam_r(void *rv,
		   void *mdata,
		   va_list ap) {
  char *name           = va_arg(ap, char *);
  struct passwd *pbuf  = va_arg(ap, struct passwd *);
  char *buf            = va_arg(ap, char *);
  size_t bsize         = va_arg(ap, size_t);         
  int *res             = va_arg(ap, int *);
  
  return ndb_getkey_r(db_getpwent,
		      path_passwd_byname,
		      (STR2OBJ) str2passwd,
		      rv, mdata,
		      name, pbuf, buf, bsize, res);
}


int
nss_ndb_getpwuid_r(void *rv,
		   void *mdata,
		   va_list ap) {
  uid_t uid            = va_arg(ap, uid_t);
  struct passwd *pbuf  = va_arg(ap, struct passwd *);
  char *buf            = va_arg(ap, char *);
  size_t bsize         = va_arg(ap, size_t);         
  int *res             = va_arg(ap, int *);

  char uidbuf[64];
  int rc;

  
  rc = snprintf(uidbuf, sizeof(uidbuf), "%u", uid);
  /* XXX Check return value */
  
  return ndb_getkey_r(db_getpwent,
		      path_passwd_byuid,
		      (STR2OBJ) str2passwd,
		      rv, mdata,
		      uidbuf, pbuf, buf, bsize, res);
}


int
nss_ndb_getpwent_r(void *rv,
		   void *mdata,
		   va_list ap) {
  struct passwd *pbuf = va_arg(ap, struct passwd *);
  char *buf           = va_arg(ap, char *);         
  size_t bsize        = va_arg(ap, size_t);         
  int *res            = va_arg(ap, int *);

#if DEBUG
  fprintf(stderr, "*** getpwent(pbuf=%p, buf=%p, bsize=%lu, res=%p)\n",
	  pbuf, buf, bsize, res);
#endif

  return ndb_getent_r(&db_getpwent,
#if BTREEVERSION >= 9
    &dbc_getpwent,
#endif
    path_passwd_byname,
    (STR2OBJ) str2passwd,
    rv, mdata,
    pbuf, buf, bsize, res);
}


int
nss_ndb_setpwent(void *rv,
		 void *mdata,
		 va_list ap) {
  int stayopen = va_arg(ap, int);

#if DEBUG
  fprintf(stderr, "*** setpwent(stayopen=%d, mdata=%d)\n",
	  stayopen, (int) mdata);
#endif
  
  return ndb_setent(&db_getpwent,
#if BTREEVERSION >= 9
    &dbc_getpwent,
#endif
    stayopen, path_passwd_byname,
    rv, mdata);
}


int
nss_ndb_endpwent(void *rv,
		 void *mdata,
		 va_list ap) {
  int *res = va_arg(ap, int *);

#if DEBUG
  fprintf(stderr, "*** endpwent()\n");
#endif
  
  return ndb_endent(&db_getpwent,
#if BTREEVERSION >= 9
    &dbc_getpwent,
#endif
    rv, mdata, res);
}


/* ---------------------------------------------------------------------- */

int
nss_ndb_getgrnam_r(void *rv,
		   void *mdata,
		   va_list ap) {
  char *name          = va_arg(ap, char *);
  struct group *gbuf  = va_arg(ap, struct group *);
  char *buf           = va_arg(ap, char *);
  size_t bsize        = va_arg(ap, size_t);         
  int *res            = va_arg(ap, int *);
  
  return ndb_getkey_r(db_getgrent,
		      path_group_byname,
		      (STR2OBJ) str2group,
		      rv, mdata,
		      name, gbuf, buf, bsize, res);
}


int
nss_ndb_getgrgid_r(void *rv,
		   void *mdata,
		   va_list ap) {
  gid_t gid          = va_arg(ap, gid_t);
  struct group *gbuf = va_arg(ap, struct group *);
  char *buf          = va_arg(ap, char *);
  size_t bsize       = va_arg(ap, size_t);
  int *res           = va_arg(ap, int *);

  char gidbuf[64];
  int rc;
  
  rc = snprintf(gidbuf, sizeof(gidbuf), "%u", gid);
  /* XXX Check return value */
  
  return ndb_getkey_r(db_getgrent,
		      path_group_bygid,
		      (STR2OBJ) str2group,
		      rv, mdata,
		      gidbuf, gbuf, buf, bsize, res);
}


int
nss_ndb_getgrent_r(void *rv,
		   void *mdata,
		   va_list ap) {
  struct group *gbuf = va_arg(ap, struct group *);
  char *buf          = va_arg(ap, char *);         
  size_t bsize       = va_arg(ap, size_t);         
  int *res           = va_arg(ap, int *);

  
#if DEBUG
  fprintf(stderr, "*** getgrent(gbuf=%p, buf=%p, bsize=%lu, res=%p)\n",
	  gbuf, buf, bsize, res);
#endif

  return ndb_getent_r(&db_getgrent,
#if BTREEVERSION >= 9
    &dbc_getgrent,
#endif
    path_group_byname,
    (STR2OBJ) str2group,
    rv, mdata,
    gbuf, buf, bsize, res);
}


int
nss_ndb_setgrent(void *rv,
		 void *mdata,
		 va_list ap) {
  int stayopen = va_arg(ap, int);

  return ndb_setent(&db_getgrent,
#if BTREEVERSION >= 9
    &dbc_getgrent,
#endif
    stayopen, path_group_byname,
    rv, mdata);
}  


int
nss_ndb_endgrent(void *rv,
		 void *mdata,
		 va_list ap) {
  int *res = va_arg(ap, int *);

#if DEBUG
  fprintf(stderr, "*** endgrent()\n");
#endif
  
  return ndb_endent(&db_getgrent,
#if BTREEVERSION >= 9
    &dbc_getgrent,
#endif
    rv, mdata, res);
}


/* ---------------------------------------------------------------------- */


static int
gr_addgid(gid_t gid,
	  gid_t *groupv,
	  int maxgrp,
	  int *groupc)
{
  int i, rc;

  /* Do not add if already added */
  for (i = 0; i < *groupc; i++) {
    if (groupv[i] == gid)
      return *groupc;
  }
  
  rc = 0;
  if (*groupc < maxgrp) {
    groupv[*groupc] = gid;
    (*groupc)++;
  } else
    rc = -1;
  
  return (rc < 0 ? rc : *groupc);
}


/* 
 * usergroups.byname.db format:
 *   user:gid,gid,gid,...
 */
int
nss_ndb_getgroupmembership(void *res,
			   void *mdata,
			   va_list ap) {
  char *name    = va_arg(ap, char *);
  gid_t pgid    = va_arg(ap, gid_t);
  gid_t *groupv = va_arg(ap, gid_t *);
  int maxgrp    = va_arg(ap, int);
  int *groupc   = va_arg(ap, int *);
  
  DB *db;
  DBT key, val;
  int rc;
  char *members, *cp;
  

  db = ndb_open(path_usergroups_byname);
  if (!db) {
    /* XXX: Fall back to looping over all entries via getgrent_r()  */
    return NS_UNAVAIL;
  }
  
  /* Add primary gid to groupv[] */
  gr_addgid(pgid, groupv, maxgrp, groupc);

  memset(&key, 0, sizeof(key));
  memset(&val, 0, sizeof(val));
  
  key.data = name;
  key.size = strlen(name);

  val.data = NULL;
  val.size = 0;
  
  rc = ndb_get(db, &key, &val, 0);
  if (rc < 0) {
    ndb_close(db);
    return NS_UNAVAIL;
  }
  else if (rc > 0) {
    ndb_close(db);
    return NS_NOTFOUND;
  }

  ndb_close(db);
  
  members = strchr(val.data, ':');
  if (members) {
    ++members;

    while ((cp = strsep(&members, ",")) != NULL) {
      gid_t gid;
      
      if (sscanf(cp, "%u", &gid) == 1)
	gr_addgid(gid, groupv, maxgrp, groupc);
    }
  }
  
  /* Let following nsswitch backend(s) add more groups(?) */
  return NS_NOTFOUND;
}


/* ---------------------------------------------------------------------- */


ns_mtab *
nss_module_register(const char *modname,
		    unsigned int *plen,
		    nss_module_unregister_fn *fptr)
{
  static ns_mtab mtab[] = {
    { "passwd", "getpwuid_r",          &nss_ndb_getpwuid_r, 0 },
    { "passwd", "getpwnam_r",          &nss_ndb_getpwnam_r, 0 },
    { "passwd", "getpwent_r",          &nss_ndb_getpwent_r, 0 },
    { "passwd", "setpwent",            &nss_ndb_setpwent, 0 },
    { "passwd", "endpwent",            &nss_ndb_endpwent, 0 },

    { "group", "getgrgid_r",           &nss_ndb_getgrgid_r, 0 },
    { "group", "getgrnam_r",           &nss_ndb_getgrnam_r, 0 },
    { "group", "getgrent_r",           &nss_ndb_getgrent_r, 0 },
    { "group", "setgrent",             &nss_ndb_setgrent, 0 },
    { "group", "endgrent",             &nss_ndb_endgrent, 0 },
    { "group", "getgroupmembership",   &nss_ndb_getgroupmembership, 0 },
  };
  
  *plen = 5+6;
  *fptr = NULL;  /* no cleanup needed */
  
  return mtab;
}

/*
 * IMPLEMENTED:
 *   passwd    getpwent(3), getpwent_r(3), getpwnam_r(3), getpwuid_r(3),
 *             setpwent(3), endpwent(3)
 *
 *   group     getgrent(3), getgrent_r(3), getgrgid_r(3), getgrnam_r(3),
 *             setgrent(3), endgrent(3)
 *
 * IN PROGRESS:
 *   group     getgroupmembership(3)
 *
 * NOT IMPLEMENTED YET:
 *   hosts     getaddrinfo(3), gethostbyaddr(3), gethostbyaddr_r(3),
 *             gethostbyname(3), gethostbyname2(3), gethostbyname_r(3),
 *             getipnodebyaddr(3), getipnodebyname(3)
 *
 *   networks  getnetbyaddr(3), getnetbyaddr_r(3), getnetbyname(3),
 *             getnetbyname_r(3)
 *
 *   shells    getusershell(3)
 *
 *   services  getservent(3)
 *
 *   rpc       getrpcbyname(3), getrpcbynumber(3), getrpcent(3)
 *
 *   proto     getprotobyname(3), getprotobynumber(3), getprotoent(3)
 *
 *   netgroup  getnetgrent(3), getnetgrent_r(3), setnetgrent(3),
 *             endnetgrent(3), innetgr(3)
 */

#if DEBUG > 1
int
main(int argc,
     char *argv[])
{
  int i;

  
  for (i = 1; i < argc; i++) {
    struct passwd pbuf, *pp;
    char buf[1024], *bptr = buf;
    size_t blen = sizeof(buf);
    int rc;
    
    rc = str2passwd(argv[i], strlen(argv[i]), &pbuf, &bptr, &blen);
    if (rc < 0) {
      fprintf(stderr, "str2passwd(\"%s\"): %s\n", argv[i], strerror(errno));
      exit(1);
    }

    pp = &pbuf;
    printf("pp->pw_name   = %s\n", pp->pw_name);
    printf("pp->pw_passwd = %s\n", pp->pw_passwd);
    printf("pp->pw_uid    = %u\n", pp->pw_uid);
    printf("pp->pw_gid    = %u\n", pp->pw_gid);
#ifdef _PATH_MASTERPASSWD
    printf("pp->pw_class  = %s\n", pp->pw_class);
    printf("pp->pw_change = %ld\n", pp->pw_change);
    printf("pp->pw_expire = %ld\n", pp->pw_expire);
#endif
    printf("pp->pw_gecos  = %s\n", pp->pw_gecos);
    printf("pp->pw_dir    = %s\n", pp->pw_dir);
    printf("pp->pw_shell  = %s\n", pp->pw_shell);
#ifdef _PATH_MASTERPASSWD
    printf("pp->pw_fields = %d\n", pp->pw_fields);
#endif
  }
  return 0;
}
#endif
