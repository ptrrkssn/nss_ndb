#
# Makefile - Build nss_ndb
#
# Copyright (c) 2017 Peter Eriksson <pen@lysator.liu.se>
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

# DEBUG="-DDEBUG=2"

VERSION=1.0.4
INCARGS=
LIBARGS=

# VERSION=1.6
# INCARGS=-I/usr/local/include/db6
# LIBARGS=-L/usr/local/lib/db6 -ldb

CPPFLAGS=-DVERSION="\"$(VERSION)\"" $(INCARGS) 
CFLAGS=-fPIC -g -Wall -DVERSION="\"$(VERSION)\"" $(INCARGS) $(DEBUG) 
LDFLAGS=-g -G $(LIBARGS) 

LIB=nss_ndb.so.$(VERSION)
LIBOBJS=nss_ndb.o

BIN=makendb
BINOBJS=makendb.o

all: $(LIB) $(BIN)

$(LIB): $(LIBOBJS)
	$(LD) $(LDFLAGS) -o $(LIB) $(LIBOBJS)

$(BIN): $(BINOBJS)
	$(CC) -g -o $(BIN) $(BINOBJS)

clean:
	-rm -f *~ \#* *.o *.core core

distclean: clean
	-rm -f *.so.* $(BIN)

install: $(LIB) $(BIN)
	$(INSTALL) -o root -g wheel -m 0444 $(LIB) $(DEST)/lib
	$(INSTALL) -o root -g wheel -m 0444 $(BIN) $(DEST)/bin
	$(INSTALL) -o root -g wheel -m 0444 makendb.1 $(DEST)/share/man/man1

push:	distclean
	git commit -a && git push

dist:	distclean
	(cd ../dist && ln -sf ../$(PACKAGE) $(PACKAGE)-$(VERSION) && tar zcf $(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)-$(VERSION)/* && rm $(PACKAGE)-$(VERSION))
