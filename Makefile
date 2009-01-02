#  Copyright (c) 2008, Thomas Jaeger <ThJaeger@gmail.com>
# 
#  Permission to use, copy, modify, and/or distribute this software for any
#  purpose with or without fee is hereby granted, provided that the above
#  copyright notice and this permission notice appear in all copies.
# 
#  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
#  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
#  MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
#  SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
#  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
#  OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
#  CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

DESTDIR  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin
ICONDIR  = $(PREFIX)/share/icons/hicolor/scalable/apps
MENUDIR  = $(PREFIX)/share/applications
LOCALEDIR= $(PREFIX)/share/locale
DFLAGS   =
OFLAGS   = -Os
AOFLAGS  = -O3
CXXFLAGS = -Wall $(DFLAGS) -DLOCALEDIR=\"$(LOCALEDIR)\" `pkg-config gtkmm-2.4 dbus-glib-1 --cflags`
LDFLAGS  = $(DFLAGS)

LIBS     = $(DFLAGS) -lboost_serialization -lXtst `pkg-config gtkmm-2.4 dbus-glib-1 --libs`

BINARY   = easystroke

CCFILES  = $(wildcard *.cc)
HFILES   = $(wildcard *.h)
OFILES   = $(patsubst %.cc,%.o,$(CCFILES)) version.o
DEPFILES = $(wildcard *.Po)
GZFILES  = $(wildcard *.gz)

VERSION  = $(shell test -e debian/changelog && grep '(.*)' debian/changelog | sed 's/.*(//' | sed 's/).*//' | head -n1 || (test -e version && cat version || git describe))
GIT      = $(wildcard .git/index version)
DIST     = easystroke-$(VERSION)
ARCH     = $(shell uname -m)

-include debug.mk

all: $(BINARY) $(MOFILES)

.PHONY: all clean snapshot release translate

clean:
	$(RM) $(OFILES) $(BINARY) $(GENFILES) $(DEPFILES) $(MANPAGE) $(GZFILES)
	$(RM) -r $(MODIRS)

include $(DEPFILES)

$(BINARY): $(OFILES)
	$(CXX) $(LDFLAGS) -o $@ $(OFILES) $(LIBS)

stroke.o: stroke.cc
	$(CXX) $(CXXFLAGS) $(AOFLAGS) -MT $@ -MMD -MP -MF $*.Po -o $@ -c $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(OFLAGS) -MT $@ -MMD -MP -MF $*.Po -o $@ -c $<

version.o: $(GIT)
	echo 'const char *version_string = "$(VERSION)";' | $(CXX) -o $@ -c -xc++ -
