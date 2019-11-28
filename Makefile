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

DEST=/usr

PACKAGE=nss_ndb

DEBUG=""
#DEBUG="-DDEBUG=2"

VERSION=1.0.18
INCARGS=
LIBARGS=

#VERSION=1.4
#INCARGS=-I/usr/local/include/db48
#LIBARGS=-L/usr/local/lib/db48 -ldb

#VERSION=1.5
#INCARGS=-I/usr/local/include/db5
#LIBARGS=-L/usr/local/lib/db5 -ldb

#VERSION=1.6
#INCARGS=-I/usr/local/include/db6
#LIBARGS=-L/usr/local/lib/db6 -ldb

CPPFLAGS=-DVERSION="\"$(VERSION)\"" $(INCARGS) 

CFLAGS=-pthread -fPIC -O -g -Wall -DVERSION="\"$(VERSION)\"" $(INCARGS) -DWITH_NSS_NDB=1

LDFLAGS=$(LIBARGS) 

LIB=nss_ndb.so.$(VERSION)
LIBOBJS=nss_ndb.o

#TESTUSER=peter86
TESTUID=1003258
TESTUSER=tesje148
TESTUID=11189
TESTGROUP=isy-ifm
#TESTGID=100001000
TESTGID=180000138
TCPASSWD=tesje148:*:11189:100000000:Testkonto Jean-Jacquesmoulis:/home/tesje148:/bin/sh
TCGROUP=isy-ifm:*:180000138:peter86
TCGRPLIST=tesje148:100000000

BINS=makendb nsstest

all: $(LIB) $(BINS)

$(LIB): $(LIBOBJS)
	$(LD) $(LDFLAGS) --shared -o $(LIB) $(LIBOBJS)

makendb.o: makendb.c ndb.h

makendb: makendb.o nss_ndb.o
	$(CC) -g -o makendb makendb.o nss_ndb.o $(LIBARGS)

clean:
	-rm -f *~ \#* *.o *.core core

distclean: clean
	-rm -f *.so.* $(BINS)

install: $(LIB) $(BINS)
	$(INSTALL) -o root -g wheel -m 0755 $(LIB) $(DEST)/lib && ln -sf $(LIB) $(DEST)/lib/nss_ndb.so.1
	$(INSTALL) -o root -g wheel -m 0755 $(BINS) $(DEST)/bin
	$(INSTALL) -o root -g wheel -m 0755 ndbsync $(DEST)/sbin
	$(INSTALL) -o root -g wheel -m 0444 makendb.1 $(DEST)/share/man/man1

push:	distclean
	git commit -a && git push

dist:	distclean
	(cd ../dist && ln -sf ../$(PACKAGE) $(PACKAGE)-$(VERSION) && tar zcf $(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)-$(VERSION)/* && rm $(PACKAGE)-$(VERSION))


nsstest:	nsstest.o nss_ndb.o
	$(CC) -g -o nsstest nsstest.o nss_ndb.o -lpthread


VALGRIND=valgrind --leak-check=full --error-exitcode=1

TESTCMD=./nsstest
TESTOPTS=

tests: t-passwd t-group t-other t-ndb_passwd

valgrind:
	$(MAKE) TESTCMD="$(VALGRIND) $(TESTCMD)" tests

t-passwd: $(NSSTEST)
	$(TESTCMD) $(TESTOPTS) -C'$(TCPASSWD)' getpwnam $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -x getpwnam no-such-user
	$(TESTCMD) $(TESTOPTS) -s -C'$(TCPASSWD)' getpwnam $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -s -x getpwnam no-such-user
	$(TESTCMD) $(TESTOPTS) -C'$(TCPASSWD)' getpwnam_r $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -x getpwnam_r no-such-user
	$(TESTCMD) $(TESTOPTS) -P10 -C'$(TCPASSWD)' getpwnam_r $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -P10 -x getpwnam_r no-such-user
	$(TESTCMD) $(TESTOPTS) -P10 -s -C'$(TCPASSWD)' getpwnam_r $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -P10 -s -x getpwnam_r no-such-user
	$(TESTCMD) $(TESTOPTS) -C'$(TCPASSWD)' getpwuid $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -x getpwuid -4711
	$(TESTCMD) $(TESTOPTS) -s -C'$(TCPASSWD)' getpwuid $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -s -x getpwuid -4711
	$(TESTCMD) $(TESTOPTS) -C'$(TCPASSWD)' getpwuid_r $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -x getpwuid_r -4711
	$(TESTCMD) $(TESTOPTS) -P10 -C'$(TCPASSWD)' getpwuid_r $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -P10 -x getpwuid_r -4711
	$(TESTCMD) $(TESTOPTS) -P10 -s -C'$(TCPASSWD)' getpwuid_r $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -P10 -s -x getpwuid_r -4711
	$(TESTCMD) $(TESTOPTS) getpwent
	$(TESTCMD) $(TESTOPTS) -s getpwent
	$(TESTCMD) $(TESTOPTS) getpwent_r
	$(TESTCMD) $(TESTOPTS) -P10 getpwent_r
	$(TESTCMD) $(TESTOPTS) -P10 -s getpwent_r

t-ndb_passwd:
	$(TESTCMD) $(TESTOPTS) -C'$(TCPASSWD)' ndb_getpwnam_r $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -x ndb_getpwnam_r no-such-user
	$(TESTCMD) $(TESTOPTS) -P10 -C'$(TCPASSWD)' ndb_getpwnam_r $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -P10 -x ndb_getpwnam_r no-such-user
	$(TESTCMD) $(TESTOPTS) -P10 -s -C'$(TCPASSWD)' ndb_getpwnam_r $(TESTUSER)
	$(TESTCMD) $(TESTOPTS) -P10 -s -x ndb_getpwnam_r no-such-user
	$(TESTCMD) $(TESTOPTS) -C'$(TCPASSWD)' ndb_getpwuid_r $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -x ndb_getpwuid_r -4711
	$(TESTCMD) $(TESTOPTS) -P10 -C'$(TCPASSWD)' ndb_getpwuid_r $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -P10 -x ndb_getpwuid_r -4711
	$(TESTCMD) $(TESTOPTS) -P10 -s -C'$(TCPASSWD)' ndb_getpwuid_r $(TESTUID)
	$(TESTCMD) $(TESTOPTS) -P10 -s -x ndb_getpwuid_r -4711

t-group: $(NSSTEST)
	$(TESTCMD) $(TESTOPTS) -C'$(TCGROUP)' getgrnam $(TESTGROUP)
	$(TESTCMD) $(TESTOPTS) -x getgrnam no-such-group
	$(TESTCMD) $(TESTOPTS) -s -C'$(TCGROUP)' getgrnam $(TESTGROUP)
	$(TESTCMD) $(TESTOPTS) -s -x getgrnam no-such-group
	$(TESTCMD) $(TESTOPTS) -C'$(TCGROUP)' getgrnam_r $(TESTGROUP)
	$(TESTCMD) $(TESTOPTS) -x getgrnam_r no-such-group
	$(TESTCMD) $(TESTOPTS) -P10 -C'$(TCGROUP)' getgrnam_r $(TESTGROUP)
	$(TESTCMD) $(TESTOPTS) -P10 -x getgrnam_r no-such-group
	$(TESTCMD) $(TESTOPTS) -P10 -s -C'$(TCGROUP)' getgrnam_r $(TESTGROUP)
	$(TESTCMD) $(TESTOPTS) -P10 -s -x getgrnam_r no-such-group
	$(TESTCMD) $(TESTOPTS) -C'$(TCGROUP)' getgrgid $(TESTGID)
	$(TESTCMD) $(TESTOPTS) -x getgrgid -4711
	$(TESTCMD) $(TESTOPTS) -s -C'$(TCGROUP)' getgrgid $(TESTGID)
	$(TESTCMD) $(TESTOPTS) -s -x getgrgid -4711
	$(TESTCMD) $(TESTOPTS) -C'$(TCGROUP)' getgrgid_r $(TESTGID)
	$(TESTCMD) $(TESTOPTS) -x getgrgid_r -4711
	$(TESTCMD) $(TESTOPTS) -P10 -C'$(TCGROUP)' getgrgid_r $(TESTGID)
	$(TESTCMD) $(TESTOPTS) -P10 -x getgrgid_r -4711
	$(TESTCMD) $(TESTOPTS) -P10 -s -C'$(TCGROUP)' getgrgid_r $(TESTGID)
	$(TESTCMD) $(TESTOPTS) -P10 -s -x getgrgid_r -4711
	$(TESTCMD) $(TESTOPTS) getgrent
	$(TESTCMD) $(TESTOPTS) -s getgrent
	$(TESTCMD) $(TESTOPTS) getgrent_r
	$(TESTCMD) $(TESTOPTS) -P10 getgrent_r
	$(TESTCMD) $(TESTOPTS) -P10 -s getgrent_r

t-other: $(NSSTEST)
	$(TESTCMD) $(TESTOPTS) -C'$(TCGRPLIST)' getgrouplist $(TESTUSER)

