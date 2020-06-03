# Makefile for the nss_ndb library & tools
#
# Copyright (c) 2017-2020 Peter Eriksson <pen@lysator.liu.se>
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
# 
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Our package name
PACKAGE=nss_ndb

# Where most of the stuff is installed
PREFIX=/usr/local

# The library where nsswitch looks for modules
NSSLIBDIR=/usr/lib

# Where we keep the database
DBDIR=/var/db/nss_ndb

# For a production system you probably want LIBDIR=$(NSSLIBDIR)
LIBDIR=$(PREFIX)/lib

BINDIR=$(PREFIX)/bin
SBINDIR=$(PREFIX)/sbin
MANDIR=$(PREFIX)/share/man


DEBUG=""
#DEBUG="-DDEBUG=2"


#### Default version, uses the old libc-builtin BerkeleyDB version

VERSION=1.0.20
INCARGS=
LIBARGS=


### BerkeleyDB 4 - uses the "db48" package

#VERSION=1.4
#INCARGS=-I/usr/local/include/db48
#LIBARGS=-L/usr/local/lib/db48 -ldb


### BerkeleyDB 5 - uses the "db5" package

#VERSION=1.5
#INCARGS=-I/usr/local/include/db5
#LIBARGS=-L/usr/local/lib/db5 -ldb


### BerkeleyDB 6 - uses the "db6" package

#VERSION=1.6
#INCARGS=-I/usr/local/include/db6
#LIBARGS=-L/usr/local/lib/db6 -ldb


CPPFLAGS=-DVERSION="\"$(VERSION)\"" $(INCARGS) 

CFLAGS=-pthread -fPIC -O -g -Wall -DVERSION="\"$(VERSION)\"" $(INCARGS) -DPATH_NSS_NDB='"$(DBDIR)"'

LDFLAGS=$(LIBARGS) 

LIB=nss_ndb.so.$(VERSION)
LIBOBJS=nss_ndb.o


# You probably want to modify these parameters before running "make tests"
# The TESTUSER should be a user defined in the NDB database

TESTUSER=$$USER
#TESTUSER=tesje148

#TESTUID=1003258
TESTUID=`id -ur $(TESTUSER)`

#TESTGID=180000138
#TESTGROUP=isy-ifm
TESTGID=`id -gr $(TESTUSER)`
TESTGROUP=`id -gn $(TESTUSER)`

#TCPASSWD=tesje148:*:11189:100000000:Testkonto Jean-Jacquesmoulis:/home/tesje148:/bin/sh
#TCGROUP=isy-ifm:*:180000138:peter86
TCPASSWD=`getent passwd \`id -un $(TESTUSER)\``
TCGROUP=`getent group \`id -gn $(TESTUSER)\``

#TCGRPLIST=tesje148:100000000
TCGRPLIST=$(TESTUSER):`id -G $(TESTUSER) | tr ' ' '\n' | sort -n | tr '\n' ',' | sed -e 's/,$$//'`

BINS=makendb nsstest

all: $(LIB) $(BINS)

$(LIB): $(LIBOBJS)
	$(LD) $(LDFLAGS) --shared -o $(LIB) $(LIBOBJS)

makendb.o: makendb.c ndb.h

nsstest.o: nsstest.c ndb.h
	$(CC) $(CFLAGS) -DWITH_NSS_NDB=1 -c nsstest.c

makendb: makendb.o nss_ndb.o
	$(CC) -g -o makendb makendb.o nss_ndb.o $(LIBARGS)

clean:
	-rm -f *~ \#* *.o *.core core

distclean: clean
	-rm -f *.so.* $(BINS)

install: install-lib install-bin install-sbin install-db

install-db:
	mkdir -p $(DBDIR)

install-bin: $(BINS)
	$(INSTALL) -o root -g wheel -m 0755 $(BINS) $(BINDIR)

install-sbin: ndbsync
	$(INSTALL) -o root -g wheel -m 0755 ndbsync $(SBINDIR)

install-man:
	$(INSTALL) -o root -g wheel -m 0444 makendb.1 $(MANDIR)/man1

install-lib: $(LIB)
	$(INSTALL) -o root -g wheel -m 0755 $(LIB) $(LIBDIR)
	@echo "Do not forget 'make install-nsslink' if you want 'nsswitch' to use the installed library."

install-nsslink: install-lib
	ln -sf $(LIBDIR)/$(LIB) $(NSSLIBDIR)/nss_ndb.so.1

pull:
	git pull

push:	distclean
	git add -A && git commit -a && git push

dist:	distclean
	(cd ../dist && ln -sf ../$(PACKAGE) $(PACKAGE)-$(VERSION) && tar zcf $(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)-$(VERSION)/* && rm $(PACKAGE)-$(VERSION))


nsstest:	nsstest.o nss_ndb.o
	$(CC) -g -o nsstest nsstest.o nss_ndb.o -lpthread -ldl $(LIBARGS)


VALGRIND=valgrind --leak-check=full --error-exitcode=1

TESTCMD=./nsstest
TESTOPTS=

check tests:	 t-info t-checkdb check-all

check-all:	 check-ndb check-nss

check-ndb:	 t-ndb_passwd t-ndb_group
check-nss:	 t-passwd t-group t-grouplist

check-passwd:    t-ndb_passwd t-passwd
check-group:     t-ndb_group t-group
check-grouplist: t-grouplist

t-info: 
	@echo "PLEASE NOTE:"
	@echo "1. You must have the NDB database files updated with content in $(DBDIR)"
	@echo "2. You should expect 'make tests' to fail for any user not in the NDB."
	@echo "To solve #2, use 'make TESTUSER=some-user-in-ndb tests'."
	@echo ""

t-checkdb:
	@for DB in passwd.byname passwd.byuid; do \
	  test -f "$(DBDIR)/$${DB}.db" || echo "Missing $(DBDIR)/$${DB}.db - you must populate the 'passwd' database"; \
	done
	@for DB in group.byname group.bygid; do \
	  test -f "$(DBDIR)/$${DB}.db" || echo "Missing $(DBDIR)/$${DB}.db - you must populate the 'group' database"; \
	done
	@for DB in group.byuser; do \
	  test -f "$(DBDIR)/$${DB}.db" || echo "Missing $(DBDIR)/$${DB}.db"; \
	done

check-valgrind valgrind:
	$(MAKE) TESTCMD="$(VALGRIND) $(TESTCMD)" TESTUSER="$(TESTUSER)" check-all

# Some arguments for "nsstest":
#
# "-x" = expect to fail (give an error if it doesn't)
# -P10" = run 10 threads in parallell (try to force multithreading bugs)
# -C"xxx" = Expected result - give an error if the output doesn't match exactly
# -s = keep the database open (setpwent/setgrent)
# -4711 = invalid user id (change if you use it :-)
# no-such-user = invalid user name (change if you use it :-)
#
t-ndb_passwd: nsstest $(DBDIR)/passwd.byuid.db $(DBDIR)/passwd.byname.db
	@echo "";echo "--- Starting 'passwd' tests directly against NDB for user $(TESTUSER) and uid $(TESTUID)";echo ""
	$(TESTCMD) $(TESTOPTS) -C"$(TCPASSWD)" ndb_getpwnam_r $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -x ndb_getpwnam_r no-such-user
	$(TESTCMD) $(TESTOPTS) -P10 -C"$(TCPASSWD)" ndb_getpwnam_r $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -P10 -x ndb_getpwnam_r no-such-user
	$(TESTCMD) $(TESTOPTS) -P10 -s -C"$(TCPASSWD)" ndb_getpwnam_r $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -P10 -s -x ndb_getpwnam_r no-such-user
	$(TESTCMD) $(TESTOPTS) -C"$(TCPASSWD)" ndb_getpwuid_r $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -x ndb_getpwuid_r -4711
	$(TESTCMD) $(TESTOPTS) -P10 -C"$(TCPASSWD)" ndb_getpwuid_r $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -P10 -x ndb_getpwuid_r -4711
	$(TESTCMD) $(TESTOPTS) -P10 -s -C"$(TCPASSWD)" ndb_getpwuid_r $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -P10 -s -x ndb_getpwuid_r -4711

t-ndb_group: nsstest $(DBDIR)/group.byname.db $(DBDIR)/group.bygid.db
	@echo "";echo "--- Starting 'group' tests directly against NDB for group $(TESTGROUP) and gid $(TESTGID)";echo ""
	$(TESTCMD) $(TESTOPTS) -C"$(TCGROUP)" ndb_getgrnam_r $(TESTGROUP)
	$(TESTCMD) $(TESTOPTS) -x ndb_getgrnam_r no-such-group
	$(TESTCMD) $(TESTOPTS) -P10 -C"$(TCGROUP)" ndb_getgrnam_r $(TESTGROUP)
	$(TESTCMD) $(TESTOPTS) -P10 -x ndb_getgrnam_r no-such-group
	$(TESTCMD) $(TESTOPTS) -P10 -s -C"$(TCGROUP)" ndb_getgrnam_r $(TESTGROUP)
	$(TESTCMD) $(TESTOPTS) -P10 -s -x ndb_getgrnam_r no-such-group
	$(TESTCMD) $(TESTOPTS) -C"$(TCGROUP)" ndb_getgrgid_r $(TESTGID)
	$(TESTCMD) $(TESTOPTS) -x ndb_getgrgid_r -4711
	$(TESTCMD) $(TESTOPTS) -P10 -C"$(TCGROUP)" ndb_getgrgid_r $(TESTGID)
	$(TESTCMD) $(TESTOPTS) -P10 -x ndb_getgrgid_r -4711
	$(TESTCMD) $(TESTOPTS) -P10 -s -C"$(TCGROUP)" ndb_getgrgid_r $(TESTGID)
	$(TESTCMD) $(TESTOPTS) -P10 -s -x ndb_getgrgid_r -4711

t-passwd: nsstest $(DBDIR)/passwd.byuid.db $(DBDIR)/passwd.byname.db
	@echo "";echo "--- Starting 'passwd' tests via NSS for user $(TESTUSER) and uid $(TESTUID)";echo ""
	$(TESTCMD) $(TESTOPTS) -C"$(TCPASSWD)" getpwnam $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -x getpwnam no-such-user
	$(TESTCMD) $(TESTOPTS) -s -C"$(TCPASSWD)" getpwnam $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -s -x getpwnam no-such-user
	$(TESTCMD) $(TESTOPTS) -C"$(TCPASSWD)" getpwnam_r $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -x getpwnam_r no-such-user
	$(TESTCMD) $(TESTOPTS) -P10 -C"$(TCPASSWD)" getpwnam_r $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -P10 -x getpwnam_r no-such-user
	$(TESTCMD) $(TESTOPTS) -P10 -s -C"$(TCPASSWD)" getpwnam_r $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -P10 -s -x getpwnam_r no-such-user
	$(TESTCMD) $(TESTOPTS) -C"$(TCPASSWD)" getpwuid $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -x getpwuid -4711
	$(TESTCMD) $(TESTOPTS) -s -C"$(TCPASSWD)" getpwuid $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -s -x getpwuid -4711
	$(TESTCMD) $(TESTOPTS) -C"$(TCPASSWD)" getpwuid_r $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -x getpwuid_r -4711
	$(TESTCMD) $(TESTOPTS) -P10 -C"$(TCPASSWD)" getpwuid_r $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -P10 -x getpwuid_r -4711
	$(TESTCMD) $(TESTOPTS) -P10 -s -C"$(TCPASSWD)" getpwuid_r $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -P10 -s -x getpwuid_r -4711
	$(TESTCMD) $(TESTOPTS) getpwent
	$(TESTCMD) $(TESTOPTS) -s getpwent
	$(TESTCMD) $(TESTOPTS) getpwent_r
	$(TESTCMD) $(TESTOPTS) -P10 getpwent_r
	$(TESTCMD) $(TESTOPTS) -P10 -s getpwent_r

t-group: nsstest $(DBDIR)/group.byname.db $(DBDIR)/group.bygid.db
	@echo "";echo "--- Starting 'group' tests via NSS for group $(TESTGROUP) and gid $(TESTGID)";echo ""
	$(TESTCMD) $(TESTOPTS) -C"$(TCGROUP)" getgrnam $(TESTGROUP)
	$(TESTCMD) $(TESTOPTS) -x getgrnam no-such-group
	$(TESTCMD) $(TESTOPTS) -s -C"$(TCGROUP)" getgrnam $(TESTGROUP)
	$(TESTCMD) $(TESTOPTS) -s -x getgrnam no-such-group
	$(TESTCMD) $(TESTOPTS) -C"$(TCGROUP)" getgrnam_r $(TESTGROUP)
	$(TESTCMD) $(TESTOPTS) -x getgrnam_r no-such-group
	$(TESTCMD) $(TESTOPTS) -P10 -C"$(TCGROUP)" getgrnam_r $(TESTGROUP)
	$(TESTCMD) $(TESTOPTS) -P10 -x getgrnam_r no-such-group
	$(TESTCMD) $(TESTOPTS) -P10 -s -C"$(TCGROUP)" getgrnam_r $(TESTGROUP)
	$(TESTCMD) $(TESTOPTS) -P10 -s -x getgrnam_r no-such-group
	$(TESTCMD) $(TESTOPTS) -C"$(TCGROUP)" getgrgid $(TESTGID)
	$(TESTCMD) $(TESTOPTS) -x getgrgid -4711
	$(TESTCMD) $(TESTOPTS) -s -C"$(TCGROUP)" getgrgid $(TESTGID)
	$(TESTCMD) $(TESTOPTS) -s -x getgrgid -4711
	$(TESTCMD) $(TESTOPTS) -C"$(TCGROUP)" getgrgid_r $(TESTGID)
	$(TESTCMD) $(TESTOPTS) -x getgrgid_r -4711
	$(TESTCMD) $(TESTOPTS) -P10 -C"$(TCGROUP)" getgrgid_r $(TESTGID)
	$(TESTCMD) $(TESTOPTS) -P10 -x getgrgid_r -4711
	$(TESTCMD) $(TESTOPTS) -P10 -s -C"$(TCGROUP)" getgrgid_r $(TESTGID)
	$(TESTCMD) $(TESTOPTS) -P10 -s -x getgrgid_r -4711
	$(TESTCMD) $(TESTOPTS) getgrent
	$(TESTCMD) $(TESTOPTS) -s getgrent
	$(TESTCMD) $(TESTOPTS) getgrent_r
	$(TESTCMD) $(TESTOPTS) -P10 getgrent_r
	$(TESTCMD) $(TESTOPTS) -P10 -s getgrent_r

t-grouplist: nsstest $(DBDIR)/group.byuser.db
	@echo "";echo "--- Starting 'grouplist' tests via NSS for user $(TESTUSER)";echo ""
	$(TESTCMD) $(TESTOPTS) -v -C"$(TCGRPLIST)" getgrouplist $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -v -C"no-such-user:-1" getgrouplist no-such-user
	$(TESTCMD) $(TESTOPTS) -v -s -C"$(TCGRPLIST)" getgrouplist $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -v -s -C"no-such-user:-1" getgrouplist no-such-user
	$(TESTCMD) $(TESTOPTS) -v -P10 -C"$(TCGRPLIST)" getgrouplist $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -v -P10 -C"no-such-user:-1" getgrouplist no-such-user
# "-x" doesn't yield the expected result here - it is NOT an error to call getgrouplist() with an
# unknown user, it just returns the "basegid" specified (which we set to -1 in our test here).
