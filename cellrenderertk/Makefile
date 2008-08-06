#  Copyright (c) 2008, Thomas Jaeger <ThJaeger@gmail.com>
# 
#  Permission to use, copy, modify, and/or distribute this software for any
#  purpose with or without fee is hereby granted, provided that the above
#  copyright notice and this permission notice appear in all copies.
# 
#  THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
#  REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
#  AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
#  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
#  LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
#  OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
#  PERFORMANCE OF THIS SOFTWARE.

OFLAGS   = -Os
CXXFLAGS = -Wall `pkg-config gtkmm-2.4 --cflags` $(OFLAGS)
CFLAGS   = -Wall `pkg-config gtk+-2.0 --cflags` $(OFLAGS)
DEFINES  = 

CCFILES  = $(wildcard *.cc)
OFILES   = $(patsubst %.cc,%.o,$(CCFILES)) cellrenderertk.o marshalers.o
DEPFILES = $(wildcard *.Po)
GENFILES = marshalers.h marshalers.c
TARGETS  = libcellrenderertk.a

all: $(TARGETS)

.PHONY: all clean

clena:	clean

clean:
	$(RM) $(OFILES) $(GENFILES) $(DEPFILES) $(TARGETS)

include $(DEPFILES)

libcellrenderertk.a: $(OFILES)
	ar -r libcellrenderertk.a $(OFILES)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -MT $@ -MMD -MP -MF $*.Po -o $@ -c $<

cellrenderertk.o: cellrenderertk.c marshalers.h marshalers.c
	$(CC) $(CFLAGS) cellrenderertk.c -o cellrenderertk.o -c

marshalers.o: marshalers.h marshalers.c
	$(CC) $(CFLAGS) marshalers.c -o marshalers.o -c

marshalers.h: marshalers.list
	glib-genmarshal --prefix=_gtk_marshal --header marshalers.list > marshalers.h

marshalers.c: marshalers.list
	glib-genmarshal --prefix=_gtk_marshal --body marshalers.list > marshalers.c
