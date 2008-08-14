#  Copyright (c) 2008, Thomas Jaeger <ThJaeger@gmail.com>
# 
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Library General Public
#  License as published by the Free Software Foundation; either
#  version 2 of the License, or (at your option) any later version.
# 
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Library General Public License for more details.
# 
#  You should have received a copy of the GNU Library General Public
#  License along with this library; if not, write to the
#  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
#  Boston, MA 02111-1307, USA.

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
