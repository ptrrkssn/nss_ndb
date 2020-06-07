/*
 * nss_ndb.c - NSS interface to Berkeley DB databases for passwd & group
 *
 * Copyright (c) 2017-2020 Peter Eriksson <pen@lysator.liu.se>
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
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/file.h>

#include "ndb.h"
#include "nss_ndb.h"

#define MAX_GETENT_SIZE 1024
#define MAX_GETOBJ_SIZE 32768

static char *path_passwd_byname     = PATH_NSS_NDB_PASSWD_BY_NAME;
static char *path_passwd_byuid      = PATH_NSS_NDB_PASSWD_BY_UID;
static char *path_group_byname      = PATH_NSS_NDB_GROUP_BY_NAME;
static char *path_group_bygid       = PATH_NSS_NDB_GROUP_BY_GID;
static char *path_usergroups_byname = PATH_NSS_NDB_USERGROUPS_BY_NAME;

static __thread NDB ndb_pwd_byname;
static __thread NDB ndb_pwd_byuid;

static __thread NDB ndb_grp_byname;
static __thread NDB ndb_grp_bygid;
static __thread NDB ndb_grp_byuser;


static __thread int f_nss_ndb_init  = 0;
#ifdef NDB_DEBUG
static __thread int f_nss_ndb_debug = 0;
#endif

#ifndef DEFAULT_WORKGROUP
#define DEFAULT_WORKGROUP NULL
#endif
#ifndef DEFAULT_REALM
#define DEFAULT_REALM NULL
#endif

static __thread const char *f_strip_workgroup = DEFAULT_WORKGROUP;
static __thread const char *f_strip_realm     = DEFAULT_REALM;

static void
_nss_ndb_init(void) {
#ifdef ENABLE_CONFIG_FILE
  FILE *fp;
#endif
#ifdef NSS_NDB_CONF_VAR
  char *bp;
#endif
  
  if (f_nss_ndb_init)
    return;
  f_nss_ndb_init = 1;

#ifdef ENABLE_CONFIG_FILE
  if ((fp = fopen(NSS_NDB_CONF_PATH, "r")) != NULL) {
    char buf[256];
    int line = 0;
    
    while (fgets(buf, sizeof(buf), fp)) {
      char *cp, *vp, *bp = buf;
      
      ++line;
      
      cp = strsep(&bp, " \t\n\r");
      if (!cp || *cp == '#')
	continue;

      vp = strsep(&bp, " \t\n\r");
      
      if (strcmp(cp, "workgroup") == 0) {
	
	if (f_strip_workgroup) {
	  free((void *) f_strip_workgroup);
	  f_strip_workgroup = NULL;
	}
	
	if (vp) {
	  if (strcmp(vp, "*") == 0)
	    vp = "";
	  f_strip_workgroup = strdup(vp);
	}
	
      } else if (strcmp(cp, "realm") == 0) {
	
	if (f_strip_realm) {
	  free((void *) f_strip_realm);
	  f_strip_realm = NULL;
	}
	
	if (vp) {
	  if (strcmp(vp, "*") == 0)
	    vp = "";
	  f_strip_realm = strdup(vp);
	} 
#ifdef NDB_DEBUG	
      } else if (strcmp(cp, "debug") == 0) {
	if (vp)
	  sscanf(vp, "%d", &f_nss_ndb_debug);
	else
	  f_nss_ndb_debug = 1;
#endif
      }
    }
    fclose(fp);
  }
#endif
  
#ifdef NSS_NDB_CONF_VAR
  if ((bp = getenv(NSS_NDB_CONF_VAR)) != NULL) {
    char *cp;
    
    while ((cp = strsep(&bp, ",")) != NULL) {
      char *vp;
      
      vp = strchr(cp, ':');
      if (vp)
	*vp++ = '\0';

#ifdef NDB_DEBUG
      if (f_nss_ndb_debug > 2)
	fprintf(stderr, "*** nss_ndb_init: getenv(\"%s\"): key = %s, val = %s\n",
		NSS_NDB_CONF_VAR,
		cp, vp ? vp : "NULL");
#endif
      
      if (strcmp(cp, "workgroup") == 0) {

	if (f_strip_workgroup) {
	  free((void *) f_strip_workgroup);
	  f_strip_workgroup = NULL;
	}
	
	if (vp) {
	  if (strcmp(vp, "*") == 0)
	    vp = "";
	  f_strip_workgroup = strdup(vp);
	}
	
      } else if (strcmp(cp, "realm") == 0) {
	
	if (f_strip_realm) {
	  free((void *) f_strip_realm);
	  f_strip_realm = NULL;
	}

	if (vp) {
	  if (strcmp(vp, "*") == 0)
	    vp = "";
	  f_strip_realm = strdup(vp);
	}
	
#ifdef NDB_DEBUG	
      } else if (strcmp(cp, "debug") == 0) {

	if (vp)
	  sscanf(vp, "%d", &f_nss_ndb_debug);
	else
	  f_nss_ndb_debug = 1;
#endif	
      }
    }
  }
#endif    
}



static void *
balloc(size_t size,
       char **buf,
       size_t *blen) {
  if (size > *blen) {
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
#ifdef __FreeBSD__
  pp->pw_fields = fc;
#endif
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
  
  free(btmp);
  gp->gr_mem[i] = NULL;
  return 0;

 Fail:
  free(btmp);
  return -1;
}



int
_ndb_get(NDB *ndb,
	 DBT *key,
	 DBT *val,
	 int flags) {
  if (!ndb)
    return -1;

  /* XXX: key.flags = DB_DBT_USERMEM; */

  if (!key->data) {
#if DB_VERSION_MAJOR < 4
    return ndb->db->seq(ndb->db, key, val, flags);
#else
    return ndb->dbc->get(ndb->dbc, key, val, flags);
#endif
  } else {
    return ndb->db->get(ndb->db,
#if DB_VERSION_MAJOR >= 4
			NULL,
#endif
			key, val, flags);
  }
}

int
_ndb_put(NDB *ndb,
	 DBT *key,
	 DBT *val,
	 int flags) {
  if (!ndb)
    return -1;

  /* XXX: key.flags = DB_DBT_USERMEM; */

  return ndb->db->put(ndb->db,
#if DB_VERSION_MAJOR >= 4
		 NULL,
#endif
		 key, val, flags);
  
}



void
_ndb_close(NDB *ndb) {
  if (!ndb)
    return;

#if NDB_DEBUG
  if (f_nss_ndb_debug)
    fprintf(stderr, "_ndb_close(%p) [%s])\n",
	  ndb,
	  ndb && ndb->path ? ndb->path : "<null>");
#endif

#if DB_VERSION_MAJOR >= 4
  if (ndb->dbc) {
    ndb->dbc->close(ndb->dbc);
  }
#endif
  
  if (ndb->db) {
    ndb->db->close(ndb->db
#if DB_VERSION_MAJOR >= 4
		   , 0
#endif
		   );
  }

  if (ndb->path) {
    free(ndb->path);
  }

  memset(ndb, 0, sizeof(*ndb));
}


int
_ndb_open(NDB *ndb,
	  const char *path,
	  int rdwr_f) {
#if DB_VERSION_MAJOR >= 4
  int ret;
#endif
  pid_t pid = getpid();

#if NDB_DEBUG
  if (f_nss_ndb_debug)
    fprintf(stderr, "_ndb_open(%p, \"%s\") -> ", ndb, path);
#endif
  
  if (!ndb->db || ndb->pid != pid) {
    if (ndb->path) {
      free(ndb->path);
    }
    memset(ndb, 0, sizeof(*ndb));
    ndb->pid = pid;
    
#if DB_VERSION_MAJOR >= 4
    ret = db_env_create(&ndb->dbe, 0);
    if (ret) {
#if NDB_DEBUG
      if (f_nss_ndb_debug)
	fprintf(stderr, "FAIL (db_env_create)\n");
#endif
      return -1;
    }
    
    ret = db_create(&ndb->db, NULL, 0);
    if (ret) {
#if NDB_DEBUG
      if (f_nss_ndb_debug)
	fprintf(stderr, "FAIL (db_create)\n");
#endif
      
      ndb->dbe->close(ndb->dbe, 0);
      ndb->dbe = NULL;
      
      return -1;
    }

#if NDB_DEBUG
    if (f_nss_ndb_debug)
      fprintf(stderr, "created -> ");
#endif

    ret = ndb->db->open(ndb->db, NULL, path, NULL, DB_BTREE, (rdwr_f ? DB_CREATE : DB_RDONLY), 0644);
    if (ret) {
#if NDB_DEBUG
      if (f_nss_ndb_debug)
	fprintf(stderr, "FAIL (db->open)\n");
#endif
      
      ndb->db->close(ndb->db, 0);
      ndb->db = NULL;
      
      ndb->dbe->close(ndb->dbe, 0);
      ndb->dbe = NULL;
      
      return -1;
    }

    /* XXX: DB_Env - do locking */
    
#else
    ndb->db = dbopen(path, (rdwr_f ? O_RDWR|O_CREAT|O_EXLOCK : O_RDONLY|O_SHLOCK), 0644, DB_BTREE, NULL);
    if (!ndb->db) {
#if NDB_DEBUG
      if (f_nss_ndb_debug)
	fprintf(stderr, "FAIL (dbopen)\n");
#endif
      return -1;
    }
#endif

  ndb->path = strdup(path);
  
#if NDB_DEBUG
  if (f_nss_ndb_debug)
    fprintf(stderr, "opened -> ");
#endif
  }
  
#if NDB_DEBUG
  if (f_nss_ndb_debug)
    fprintf(stderr, "%p\n", ndb);
#endif
  return 0;
}


static int
_ndb_getkey_r(NDB *ndb,
	     const char *path,
	     STR2OBJ str2obj,
	     void *rv,
	     void *mdata,
	     char *name,
	     void *pbuf,
	     char *buf,
	     size_t bsize,
	     int *res) {
  int ec = NS_SUCCESS;
  void **ptr = rv;
  DBT key, val;
  int rc;

  
  if (_ndb_open(ndb, path, 0) < 0)
    return NS_UNAVAIL;
  
  *ptr = 0;
  
  memset(&key, 0, sizeof(key));
  memset(&val, 0, sizeof(val));
  
  key.data = name;
  key.size = strlen(name);
  
  rc = _ndb_get(ndb, &key, &val, 0);
  if (rc < 0) {
    *res = errno;
    ec = NS_UNAVAIL;
  } else if (rc > 0)
    ec = NS_NOTFOUND;
  else {
    if ((*str2obj)(val.data, val.size, pbuf, &buf, &bsize, MAX_GETOBJ_SIZE) < 0) {
      *res = errno;
      ec = NS_UNAVAIL;
    } else
      *ptr = pbuf;
  }

  if (!ndb->stayopen)
    _ndb_close(ndb);
  
  return ec;
}
  

static int
_ndb_getent_r(NDB *ndb,
	     const char *path,
	     STR2OBJ str2obj,
	     void *rv,
	     void *mdata,
	     void *pbuf,
	     char *buf,
	     size_t bsize,
	     int *res) {
  void **ptr = rv;
  int rc, ec = NS_SUCCESS;
  DBT key, val;
  

  if (_ndb_open(ndb, path, 0) < 0) 
    return NS_UNAVAIL;

#if DB_VERSION_MAJOR >= 4
  if (!ndb->dbc) {
    if ((rc = ndb->db->cursor(ndb->db, NULL, &ndb->dbc, 0)) || !ndb->dbc) {
      return NS_UNAVAIL;
    }
  }
#endif
  
  *ptr = 0;

  memset(&key, 0, sizeof(key));
  memset(&val, 0, sizeof(val));

#if DB_VERSION_MAJOR >= 4
  rc = ndb->dbc->get(ndb->dbc, &key, &val, ndb->prev_f ? DB_PREV : DB_NEXT);
#else
  rc = ndb->db->seq(ndb->db, &key, &val, ndb->prev_f ? R_PREV : R_NEXT);
#endif

  ndb->prev_f = 0;
  
  if (rc < 0) {
    _ndb_close(ndb);
    *res = errno;
    ec = NS_UNAVAIL;
  } else if (rc > 0)
    ec = NS_NOTFOUND;
  else {
  
    if ((*str2obj)(val.data, val.size, pbuf, &buf, &bsize, MAX_GETENT_SIZE) < 0) {
      *res = errno;
   
      /* If errno == ERANGE (data can't fit in buffer), retry same record next time */
      if (errno == ERANGE)
	ndb->prev_f = 1;
      
      ec = NS_NOTFOUND;
    } else
      *ptr = pbuf;
  }
  
  return ec;
}


int
_ndb_setent(NDB *ndb,
	    int stayopen,
	    const char *path) {
  _ndb_close(ndb);
  
  if (_ndb_open(ndb, path, 0) < 0)
    return NS_UNAVAIL;

  ndb->stayopen = stayopen;
  return NS_SUCCESS;
}



int
_ndb_endent(NDB *ndb) {
  _ndb_close(ndb);
  return NS_SUCCESS;
}



int
nss_ndb_getpwnam_r(void *rv,
		   void *mdata,
		   va_list ap) {
  char *name           = va_arg(ap, char *);
  struct passwd *pbuf  = va_arg(ap, struct passwd *);
  char *buf            = va_arg(ap, char *);
  size_t bsize         = va_arg(ap, size_t);         
  int *res             = va_arg(ap, int *);
  char *cp;
  char *nbuf = NULL;
  int rc;


  _nss_ndb_init();
  
  if (f_strip_workgroup) {
    size_t len;
    
    /* Strip AD workgroup prefix if specified */
    cp = strchr(name, '\\');
    if (cp && (f_strip_workgroup[0] == '\0' || /* Accept all workgroups */
	       cp-name == 0 || /* Empty workgroup specified (\user) */
	       ((len = strlen(f_strip_workgroup)) == cp-name && 
		strncasecmp(name, f_strip_workgroup, len) == 0))) /* Exact match */
      name = cp+1;
  }
  
  if (f_strip_realm) {
    /* Strip Kerberos realm suffix if specified */
    cp = strrchr(name, '@');
    if (cp && (f_strip_realm[0] == '\0' ||        /* Accept all realms */
	       strcasecmp(cp+1, f_strip_realm) == 0)) /* Exact match */
      name = nbuf = strndup(name, cp-name);
  }
  
  rc = _ndb_getkey_r(&ndb_pwd_byname,
		     path_passwd_byname,
		     (STR2OBJ) str2passwd,
		     rv, mdata,
		     name, pbuf, buf, bsize, res);

  if (nbuf)
    free(nbuf);
  
  return rc;
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

  
  (void) snprintf(uidbuf, sizeof(uidbuf), "%u", uid);
  /* XXX Check return value */
  
  return _ndb_getkey_r(&ndb_pwd_byuid,
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

  return _ndb_getent_r(&ndb_pwd_byname,
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

  return _ndb_setent(&ndb_pwd_byname,
		    stayopen,
		    path_passwd_byname);
}


int
nss_ndb_endpwent(void *rv,
		 void *mdata,
		 va_list ap) {
#if 0
  int *res = va_arg(ap, int *);
#endif
  
  return _ndb_endent(&ndb_pwd_byname);
}



int
nss_ndb_getgrnam_r(void *rv,
		   void *mdata,
		   va_list ap) {
  char *name          = va_arg(ap, char *);
  struct group *gbuf  = va_arg(ap, struct group *);
  char *buf           = va_arg(ap, char *);
  size_t bsize        = va_arg(ap, size_t);         
  int *res            = va_arg(ap, int *);
  char *cp;
  char *nbuf = NULL;
  int rc;


  _nss_ndb_init();
  
  if (f_strip_workgroup) {
    size_t len;
    
    /* Strip AD workgroup prefix if specified */
    cp = strchr(name, '\\');
    if (cp && (f_strip_workgroup[0] == '\0' || /* Accept all workgroups */
	       cp-name == 0 ||                 /* Empty specified workgroup */
	       ((len = strlen(f_strip_workgroup)) == cp-name && 
		strncasecmp(name, f_strip_workgroup, len) == 0))) /* Exact match */
      name = cp+1;
  }
  
  if (f_strip_realm) {
    /* Strip realm suffix if specified */
    cp = strrchr(name, '@');
    if (cp && (f_strip_realm[0] == '\0' ||        /* Accept all realms */
	       strcasecmp(cp+1, f_strip_realm) == 0)) /* Exact match */
      name = nbuf = strndup(name, cp-name);
  }
  
  rc = _ndb_getkey_r(&ndb_grp_byname,
		      path_group_byname,
		      (STR2OBJ) str2group,
		      rv, mdata,
		      name, gbuf, buf, bsize, res);

  if (nbuf)
    free(nbuf);
  
  return rc;
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
  
  (void) snprintf(gidbuf, sizeof(gidbuf), "%u", gid);
  /* XXX Check return value */
  
  return _ndb_getkey_r(&ndb_grp_bygid,
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

  
  return _ndb_getent_r(&ndb_grp_byname,
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

  return _ndb_setent(&ndb_grp_byname,
		     stayopen,
		     path_group_byname);
}


int
nss_ndb_endgrent(void *rv,
		 void *mdata,
		 va_list ap) {
#if 0
  int *res = va_arg(ap, int *);
#endif
  
  return _ndb_endent(&ndb_grp_byname);
}



static int
gr_addgid(gid_t gid,
	  gid_t *groupv,
	  int maxgrp,
	  int *groupc)
{
  int i;

  
  /* Do not add if already added */
  for (i = 0; i < *groupc; i++) {
    if (groupv[i] == gid)
      return 0;
  }
  
  if (*groupc >= maxgrp)
    return -1;
    
  groupv[*groupc] = gid;
  ++*groupc;
  return 1;
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
  
  DBT key, val;
  int rc;
  char *members, *cp;
  char *nbuf = NULL;
  

  if (name == NULL)
    return NS_NOTFOUND;
  
  if (_ndb_open(&ndb_grp_byuser, path_usergroups_byname, 0) < 0) {
    /* Fall back to looping over all entries via getgrent_r() - slooooow */
    return NS_UNAVAIL;
  }

  /* Add primary gid to groupv[] */
  (void) gr_addgid(pgid, groupv, maxgrp, groupc);
  

  _nss_ndb_init();
  
  if (f_strip_workgroup) {
    size_t len;
    
    /* Strip AD workgroup prefix if specified */
    cp = strchr(name, '\\');
    if (cp && (f_strip_workgroup[0] == '\0' || /* Accept all workgroups */
	       cp-name == 0 || /* Empty workgroup specified (\user) */
	       ((len = strlen(f_strip_workgroup)) == cp-name && 
		strncasecmp(name, f_strip_workgroup, len) == 0))) /* Exact match */
      name = cp+1;
  }
  
  if (f_strip_realm) {
    /* Strip Kerberos realm suffix if specified */
    cp = strrchr(name, '@');
    if (cp && (f_strip_realm[0] == '\0' ||        /* Accept all realms */
	       strcasecmp(cp+1, f_strip_realm) == 0)) /* Exact match */
      name = nbuf = strndup(name, cp-name);
  }

  
  memset(&key, 0, sizeof(key));
  memset(&val, 0, sizeof(val));
  
  key.data = name;
  key.size = strlen(name);

  val.data = NULL;
  val.size = 0;
  
  rc = _ndb_get(&ndb_grp_byuser, &key, &val, 0);
  if (rc < 0) {
    _ndb_close(&ndb_grp_byuser);
    if (nbuf)
      free(nbuf);
    return NS_UNAVAIL;
  }
  else if (rc > 0) {
    _ndb_close(&ndb_grp_byuser);
    if (nbuf)
      free(nbuf);
    return NS_NOTFOUND;
  }

  /* Should not happen */
  if (val.data == NULL) {
    _ndb_close(&ndb_grp_byuser);
    if (nbuf)
      free(nbuf);
    return NS_UNAVAIL;
  }

  members = memchr(val.data, ':', val.size);
  if (members) {
    ++members;

    while ((cp = strsep(&members, ",")) != NULL) {
      gid_t gid;
      
      if (sscanf(cp, "%u", &gid) == 1) {
	(void) gr_addgid(gid, groupv, maxgrp, groupc);
      }
    }
  }

  _ndb_close(&ndb_grp_byuser);
  
  if (nbuf)
    free(nbuf);
	
  /* Let following nsswitch backend(s) add more groups(?) */
  return NS_NOTFOUND;
}


#ifdef __FreeBSD__
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
    { "group", "getgroupmembership",   &nss_ndb_getgroupmembership, 0 }, /* aka getgrouplist */
  };
  
  *plen = sizeof(mtab)/sizeof(mtab[0]);
  *fptr = NULL;  /* no cleanup needed */
  
  return mtab;
}
#endif

/*
 * IMPLEMENTED:
 *   passwd    getpwent(3), getpwent_r(3), getpwnam_r(3), getpwuid_r(3),
 *             setpwent(3), endpwent(3)
 *
 *   group     getgrent(3), getgrent_r(3), getgrgid_r(3), getgrnam_r(3),
 *             setgrent(3), endgrent(3)
 *
 *   group     getgrouplist(3)
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

