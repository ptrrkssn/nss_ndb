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

VERSION=1.0.10
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

CFLAGS=-fPIC -g -Wall -DVERSION="\"$(VERSION)\"" $(INCARGS)

#LDFLAGS=-g -G $(LIBARGS) 
LDFLAGS=--shared $(LIBARGS) 

LIB=nss_ndb.so.$(VERSION)
LIBOBJS=nss_ndb.o

BINS=makendb
TESTS=t_getgrnam t_getgrent t_getpwnam t_getpwent

all: $(LIB) $(BINS)

$(LIB): $(LIBOBJS)
	$(LD) $(LDFLAGS) -o $(LIB) $(LIBOBJS)

makendb.o: makendb.c ndb.h

makendb: makendb.o nss_ndb.o
	$(CC) -g -o makendb makendb.o nss_ndb.o $(LIBARGS)

clean:
	-rm -f *~ \#* *.o *.core core

distclean: clean
	-rm -f *.so.* $(BINS) $(TESTS)

install: $(LIB) $(BINS)
	$(INSTALL) -o root -g wheel -m 0755 $(LIB) $(DEST)/lib
	$(INSTALL) -o root -g wheel -m 0755 $(BINS) $(DEST)/bin
	$(INSTALL) -o root -g wheel -m 0444 makendb.1 $(DEST)/share/man/man1

push:	distclean
	git commit -a && git push

dist:	distclean
	(cd ../dist && ln -sf ../$(PACKAGE) $(PACKAGE)-$(VERSION) && tar zcf $(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)-$(VERSION)/* && rm $(PACKAGE)-$(VERSION))

tests:	$(TESTS)

t_getgrnam:	t_getgrnam.o 
	$(CC) -g -o t_getgrnam t_getgrnam.o 

t_getgrent:	t_getgrent.o 
	$(CC) -g -o t_getgrent t_getgrent.o 

t_getpwnam:	t_getpwnam.o 
	$(CC) -g -o t_getpwnam t_getpwnam.o 

t_getpwent:	t_getpwent.o 
	$(CC) -g -o t_getpwent t_getpwent.o 

