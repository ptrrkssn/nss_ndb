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
#include <sys/types.h>
#include <stdarg.h>
#include <nsswitch.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <db.h>
#include <fcntl.h>
#include <limits.h>


#ifndef PATH_NSS_NDB
#define PATH_NSS_NDB "/var/db/nss_ndb"
#endif

#define PATH_NSS_NDB_PASSWD_BY_UID  PATH_NSS_NDB "/passwd.byuid.db"
#define PATH_NSS_NDB_PASSWD_BY_NAME PATH_NSS_NDB "/passwd.byname.db"

#define PATH_NSS_NDB_GROUP_BY_GID   PATH_NSS_NDB "/group.bygid.db"
#define PATH_NSS_NDB_GROUP_BY_NAME  PATH_NSS_NDB "/group.byname.db"


static __thread DB *db_passwd_byuid = NULL;
static __thread DB *db_passwd_byname = NULL;

static __thread DB *db_group_bygid = NULL;
static __thread DB *db_group_byname = NULL;


static __thread char *path_passwd_byname = PATH_NSS_NDB_PASSWD_BY_NAME;
static __thread char *path_passwd_byuid  = PATH_NSS_NDB_PASSWD_BY_UID;

static __thread char *path_group_byname  = PATH_NSS_NDB_GROUP_BY_NAME;
static __thread char *path_group_bygid   = PATH_NSS_NDB_GROUP_BY_GID;



static void
open_passwd_db(void) {
  if (!db_passwd_byuid)
      db_passwd_byuid = dbopen(path_passwd_byuid, O_RDONLY, 0, DB_BTREE, NULL);
  
  if (!db_passwd_byname)
      db_passwd_byname = dbopen(path_passwd_byname, O_RDONLY, 0, DB_BTREE, NULL);
}


static void
close_passwd_db(void) {
  if (db_passwd_byuid) {
    db_passwd_byuid->close(db_passwd_byuid);
    db_passwd_byuid = NULL;
  }
  if (db_passwd_byname) {
    db_passwd_byname->close(db_passwd_byname);
    db_passwd_byname = NULL;
  }
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
  if (len > *blen)
    return NULL;

  memcpy(rstr = *buf, str, len);
  *blen -= len;
  *buf  += len;

  return rstr;
}


static void *
balloc(size_t size,
       char **buf,
       size_t *blen) {
  if (size > *blen)
    return NULL;

  void *rptr = *buf;
  *buf += size;
  *blen -= size;

  return rptr;
}


static struct passwd *
str2passwd(char *str,
	   struct passwd *pp,
	   char **buf,
	   size_t *blen) {
  static struct passwd pbuf;
  char *tmp, *btmp;

  if (!str)
    return NULL;
  
  if (!pp)
    pp = &pbuf;

  memset(pp, 0, sizeof(*pp));

  str = btmp = strdup(str);
  if (!str)
    return NULL;
  
  pp->pw_name = strbdup(strsep(&str, ":"), buf, blen);
  if (!pp->pw_name)
    goto Fail;
  
  pp->pw_passwd = strbdup(strsep(&str, ":"), buf, blen);
  if (!pp->pw_passwd)
    goto Fail;
  
  tmp = strsep(&str, ":"); 
  if (!tmp || sscanf(tmp, "%u", &pp->pw_uid) != 1)
    goto Fail;
  
  tmp = strsep(&str, ":");
  if (!tmp || sscanf(tmp, "%u", &pp->pw_gid) != 1)
    goto Fail;
  
  pp->pw_class = strbdup(strsep(&str, ":"), buf, blen);
  if (!pp->pw_class)
    goto Fail;
  
  tmp = strsep(&str, ":");
  if (!tmp || sscanf(tmp, "%lu", &pp->pw_change) != 1)
    goto Fail;
  
  tmp = strsep(&str, ":");
  if (!tmp || sscanf(tmp, "%lu", &pp->pw_expire) != 1)
    goto Fail;

  pp->pw_gecos = strbdup(strsep(&str, ":"), buf, blen);
  if (!pp->pw_gecos)
    goto Fail;

  pp->pw_dir = strbdup(strsep(&str, ":"), buf, blen);
  if (!pp->pw_dir)
    goto Fail;
  
  pp->pw_shell = strbdup(strsep(&str, ":"), buf, blen);
  if (!pp->pw_shell)
    goto Fail;
  
  pp->pw_fields = 10;
  return pp;

 Fail:
  free(btmp);
  return NULL;
}


static int
nss_ndb_getpwuid_r(void *rv, void *mdata, va_list ap) {
  struct passwd **ptr = rv;
  uid_t uid;
  struct passwd *pbuf;
  char *buf;
  size_t bsize;
  char uidbuf[64];
  
  DBT key, val;
  int rc;
  int *res;
  
  uid   = va_arg(ap, uid_t);         
  pbuf  = va_arg(ap, struct passwd *);
  buf   = va_arg(ap, char *);         
  bsize = va_arg(ap, size_t);         
  res   = va_arg(ap, int *);

  *ptr = 0;
  
  open_passwd_db();
  if (!db_passwd_byuid)
    return NS_UNAVAIL;

  rc = snprintf(uidbuf, sizeof(uidbuf), "%u", uid);
  /* XXX Check return value */
  
  key.data = uidbuf;
  key.size = strlen(uidbuf);

  val.data = NULL;
  val.size = 0;
  
  rc = db_passwd_byuid->get(db_passwd_byuid, &key, &val, 0);
  
  if (rc < 0) {
    *res = errno;
    return NS_UNAVAIL;
  }
  else if (rc > 0)
    return NS_NOTFOUND;

  *ptr = str2passwd(val.data, pbuf, &buf, &bsize);
  if (*ptr)
    return NS_SUCCESS;
  
  return NS_NOTFOUND;
}

static int
nss_ndb_getpwnam_r(void *rv, void *mdata, va_list ap) {
  struct passwd **ptr = rv;
  struct passwd *pbuf;
  char *buf;
  size_t bsize;
  char *name;
  
  DBT key, val;
  int rc;
  int *res;
  
  name  = va_arg(ap, char *);
  pbuf  = va_arg(ap, struct passwd *);     /* struct passwd *pwd */
  buf   = va_arg(ap, char *);             /* char *buffer */
  bsize = va_arg(ap, size_t);              /* size_t bufsize */
  res   = va_arg(ap, int *);

  *ptr = 0;
  
  open_passwd_db();
  if (!db_passwd_byname)
    return NS_UNAVAIL;
  
  key.data = name;
  key.size = strlen(name);

  val.data = NULL;
  val.size = 0;
  
  rc = db_passwd_byname->get(db_passwd_byname, &key, &val, 0);
  *res = errno;
  
  if (rc < 0)
    return NS_UNAVAIL;
  else if (rc > 0)
    return NS_NOTFOUND;

  *ptr = str2passwd(val.data, pbuf, &buf, &bsize);
  if (*ptr)
    return NS_SUCCESS;
  
  return NS_NOTFOUND;
}

static int
nss_ndb_getpwent_r(void *rv, void *mdata, va_list ap) {
  struct passwd **ptr = rv;
  
  struct passwd *pbuf;
  char *buf;
  size_t bsize;
  int *res;
  
  int rc;
  DBT key, val;
  
  
  pbuf  = va_arg(ap, struct passwd *);     /* struct passwd *pwd */
  buf   = va_arg(ap, char *);             /* char *buffer */
  bsize = va_arg(ap, size_t);              /* size_t bufsize */
  res   = va_arg(ap, int *);

  *ptr = 0;

  open_passwd_db();
  if (!db_passwd_byname)
    return NS_UNAVAIL;

 Next:
  key.data = NULL;
  key.size = 0;

  val.data = NULL;
  val.size = 0;
  
  rc = db_passwd_byname->seq(db_passwd_byname, &key, &val, R_NEXT);
  *res = errno;
  
  if (rc < 0)
    return NS_UNAVAIL;
  else if (rc > 0)
    return NS_NOTFOUND;

  *ptr = str2passwd(val.data, pbuf, &buf, &bsize);
  if (*ptr)
    return NS_SUCCESS;

#if DEBUG
    fprintf(stderr, "*** nss_ndb_getpwent_r: str2passwd failed on: %s\n", val.data);
#endif
    
  goto Next;
}

static int
nss_ndb_getpwent(void *rv, void *mdata, va_list ap) {
  struct passwd **ptr = rv;
  
  static struct passwd pbuf;
  char buf[2048], *bufptr;
  size_t buflen;
  int *res;
  
  int rc;
  DBT key, val;
  
  
  res = va_arg(ap, int *);
  *ptr = 0;

  open_passwd_db();
  if (!db_passwd_byname)
    return NS_UNAVAIL;
  
  key.data = NULL;
  key.size = 0;

  val.data = NULL;
  val.size = 0;
  
  rc = db_passwd_byname->seq(db_passwd_byname, &key, &val, R_NEXT);
  *res = errno;
  
  if (rc < 0)
    return NS_UNAVAIL;
  else if (rc > 0)
    return NS_NOTFOUND;

  bufptr = buf;
  buflen = sizeof(buf);
  *ptr = str2passwd(val.data, &pbuf, &bufptr, &buflen);
  if (*ptr)
    return NS_SUCCESS;
  
  return NS_NOTFOUND;
}


static int
nss_ndb_setpwent(void *rv, void *mdata, va_list ap) {
  int *res;

  res   = va_arg(ap, int *);

  open_passwd_db();
  if (!db_passwd_byuid) {
    *res = errno;
    return NS_UNAVAIL;
  }
  
  return NS_SUCCESS;
}

static int
nss_ndb_endpwent(void *rv, void *mdata, va_list ap) {
  int *res;

  res = va_arg(ap, int *);

  close_passwd_db();
  return NS_SUCCESS;
}


static void
open_group_db(void) {
  if (!db_group_bygid)
      db_group_bygid = dbopen(path_group_bygid, O_RDONLY, 0, DB_BTREE, NULL);
  
  if (!db_group_byname)
      db_group_byname = dbopen(path_group_byname, O_RDONLY, 0, DB_BTREE, NULL);
}


static void
close_group_db(void) {
  if (db_group_bygid) {
    db_group_bygid->close(db_group_bygid);
    db_group_bygid = NULL;
  }
  if (db_group_byname) {
    db_group_byname->close(db_group_byname);
    db_group_byname = NULL;
  }
}



static struct group *
str2group(char *str,
	  struct group *gp,
	  char **buf,
	  size_t *blen) {
  static struct group gbuf;
  char *tmp, *members, *btmp;
  int i;


  if (!str)
    return NULL;
  
  if (!gp)
    gp = &gbuf;

  str = btmp = strdup(str);
  if (!str)
    return NULL;
  
  memset(gp, 0, sizeof(*gp));
  
  gp->gr_name = strbdup(strsep(&str, ":"), buf, blen);
  if (!gp->gr_name)
    goto Fail;
  
  gp->gr_passwd = strbdup(strsep(&str, ":"), buf, blen);
  if (!gp->gr_passwd)
    goto Fail;
  
  tmp = strsep(&str, ":");
  if (!tmp || sscanf(tmp, "%u", &gp->gr_gid) != 1)
    goto Fail;
  
  members = strsep(&str, ":");

  gp->gr_mem = balloc(64*sizeof(char *), buf, blen);
  i = 0;
  while ((tmp = strsep(&members, ",")) != NULL) {
    gp->gr_mem[i++] = strbdup(tmp, buf, blen);
    /* XXX: Handle overflow */
  }
  gp->gr_mem[i] = NULL;

  return gp;

 Fail:
  free(btmp);
  return NULL;
}


static int
nss_ndb_getgrgid_r(void *rv, void *mdata, va_list ap) {
  gid_t gid;
  struct group **ptr = rv;
  struct group *gbuf;
  char *buf;
  size_t bsize;
  char gidbuf[64];
  DBT key, val;
  int rc;
  int *res;
  
  gid   = va_arg(ap, gid_t);
  gbuf  = va_arg(ap, struct group *);
  buf   = va_arg(ap, char *);
  bsize = va_arg(ap, size_t);
  res   = va_arg(ap, int *);

  *ptr = 0;
  
  open_group_db();
  if (!db_group_bygid)
    return NS_UNAVAIL;

  rc = snprintf(gidbuf, sizeof(gidbuf), "%u", gid);
  /* XXX: Check rc return value */
  
  key.data = gidbuf;
  key.size = strlen(gidbuf);

  val.data = NULL;
  val.size = 0;
  
  rc = db_group_bygid->get(db_group_bygid, &key, &val, 0);
  
  if (rc < 0) {
    *res = errno;
    return NS_UNAVAIL;
  }
  else if (rc > 0)
    return NS_NOTFOUND;

  *ptr = str2group(val.data, gbuf, &buf, &bsize);
  if (*ptr)
    return NS_SUCCESS;
  
  return NS_NOTFOUND;
}

static int
nss_ndb_getgrnam_r(void *rv, void *mdata, va_list ap) {
  struct group **ptr = rv;
  struct group *gbuf;
  char *buf;
  size_t bsize;
  char *name;
  
  DBT key, val;
  int rc;
  int *res;
  
  name  = va_arg(ap, char *);
  gbuf  = va_arg(ap, struct group *);
  buf   = va_arg(ap, char *);        
  bsize = va_arg(ap, size_t);        
  res   = va_arg(ap, int *);

  *ptr = 0;
  
  open_group_db();
  if (!db_group_byname)
    return NS_UNAVAIL;
  
  key.data = name;
  key.size = strlen(name);

  val.data = NULL;
  val.size = 0;
  
  rc = db_group_byname->get(db_group_byname, &key, &val, 0);
  *res = errno;
  
  if (rc < 0)
    return NS_UNAVAIL;
  else if (rc > 0)
    return NS_NOTFOUND;

  *ptr = str2group(val.data, gbuf, &buf, &bsize);
  if (*ptr)
    return NS_SUCCESS;
  
  return NS_NOTFOUND;
}

static int
nss_ndb_getgrent_r(void *rv, void *mdata, va_list ap) {
  struct group **ptr = rv;
  
  struct group *gbuf;
  char *buf;
  size_t bsize;
  int *res;
  
  int rc;
  DBT key, val;
  
  
  gbuf  = va_arg(ap, struct group *);
  buf   = va_arg(ap, char *);        
  bsize = va_arg(ap, size_t);        
  res   = va_arg(ap, int *);

  *ptr = 0;

  open_group_db();
  if (!db_group_byname)
    return NS_UNAVAIL;

 Next:
  key.data = NULL;
  key.size = 0;

  val.data = NULL;
  val.size = 0;
  
  rc = db_group_byname->seq(db_group_byname, &key, &val, R_NEXT);
  *res = errno;

  if (rc < 0)
    return NS_UNAVAIL;
  else if (rc > 0)
    return NS_NOTFOUND;

  *ptr = str2group(val.data, gbuf, &buf, &bsize);
  if (*ptr)
    return NS_SUCCESS;

#if DEBUG
    fprintf(stderr, "*** nss_ndb_getgrent_r: str2group failed on: %s\n", val.data);
#endif
    
  goto Next;
}

static int
nss_ndb_getgrent(void *rv, void *mdata, va_list ap) {
  struct group **ptr = rv;
  
  static struct group pbuf;
  char buf[2048], *bufptr;
  size_t buflen;
  int *res;
  
  int rc;
  DBT key, val;
  
  
  res = va_arg(ap, int *);
  *ptr = 0;

  open_group_db();
  if (!db_group_byname)
    return NS_UNAVAIL;

 Next:
  key.data = NULL;
  key.size = 0;

  val.data = NULL;
  val.size = 0;
  
  rc = db_group_byname->seq(db_group_byname, &key, &val, R_NEXT);
  *res = errno;
  
  if (rc < 0)
    return NS_UNAVAIL;
  else if (rc > 0)
    return NS_NOTFOUND;

  bufptr = buf;
  buflen = sizeof(buf);
  *ptr = str2group(val.data, &pbuf, &bufptr, &buflen);
  if (*ptr)
    return NS_SUCCESS;

  goto Next;
}


static int
nss_ndb_setgrent(void *rv, void *mdata, va_list ap) {
  int *res;

  res   = va_arg(ap, int *);

  open_group_db();
  if (!db_group_bygid) {
    *res = errno;
    return NS_UNAVAIL;
  }
  
  return NS_SUCCESS;
}

static int
nss_ndb_endgrent(void *rv, void *mdata, va_list ap) {
  int *res;

  res = va_arg(ap, int *);

  close_group_db();
  return NS_SUCCESS;
}




ns_mtab *
nss_module_register(const char *modname,
		    unsigned int *plen,
		    nss_module_unregister_fn *fptr)
{
  static ns_mtab mtab[] = {
    { "passwd", "getpwuid_r", &nss_ndb_getpwuid_r, 0 },
    { "passwd", "getpwnam_r", &nss_ndb_getpwnam_r, 0 },
    { "passwd", "getpwent_r", &nss_ndb_getpwent_r, 0 },
    { "passwd", "getpwent",   &nss_ndb_getpwent, 0 },
    { "passwd", "setpwent",   &nss_ndb_setpwent, 0 },
    { "passwd", "endpwent",   &nss_ndb_endpwent, 0 },

    { "group", "getgrgid_r", &nss_ndb_getgrgid_r, 0 },
    { "group", "getgrnam_r", &nss_ndb_getgrnam_r, 0 },
    { "group", "getgrent_r", &nss_ndb_getgrent_r, 0 },
    { "group", "getgrent",   &nss_ndb_getgrent, 0 },
    { "group", "setgrent",   &nss_ndb_setgrent, 0 },
    { "group", "endgrent",   &nss_ndb_endgrent, 0 },
  };
  
  *plen = 12;    /* one ns_mtab item */
  *fptr = NULL;  /* no cleanup needed */
  
  return mtab;
}
