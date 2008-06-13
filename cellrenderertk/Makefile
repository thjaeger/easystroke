OFLAGS   = -Os
CXXFLAGS = -Wall `pkg-config gtkmm-2.4 libglademm-2.4 --cflags` $(OFLAGS)
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
