PREFIX   = /usr/local
DFLAGS   = #-ggdb #-pg
OFLAGS   = -Os
AOFLAGS  = -O3
CXXFLAGS = -Wall $(DFLAGS) `pkg-config gtkmm-2.4 libglademm-2.4 --cflags`
TARGETS  = easystroke
DEFINES  = 

LIBS     = $(DFLAGS) -lboost_serialization -lXtst `pkg-config gtkmm-2.4 libglademm-2.4 gthread-2.0 --cflags --libs`
INCLUDES = 

BINARY   = easystroke

CCFILES  = $(wildcard *.cc) $(wildcard cellrenderertk/*.cc)
OFILES   = $(patsubst %.cc,%.o,$(CCFILES)) dsimple.o gui.o cellrenderertk/cellrenderertk.o cellrenderertk/marshalers.o
DEPFILES = $(wildcard *.Po) $(wildcard cellrenderertk/*.Po)
GENFILES = cellrenderertk/marshalers.h cellrenderertk/marshalers.c

all: $(TARGETS)

.PHONY: all clean $(BINARY)

clena:	clean

clean:
	$(RM) $(OFILES) $(BINARY) $(GENFILES) $(DEPFILES) gui.c gui.o

include $(DEPFILES)

easystroke: $(OFILES)
	$(CXX) $(LDFLAGS) -o $(BINARY) $(OFILES) $(LIBS)

stroke.o: stroke.cc
	$(CXX) $(CXXFLAGS) $(AOFLAGS) $(DEFINES) $(INCLUDES) -MT $@ -MMD -MP -MF $*.Po -o $@ -c $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(OFLAGS) $(DEFINES) $(INCLUDES) -MT $@ -MMD -MP -MF $*.Po -o $@ -c $<

gui.c: gui.glade
	echo "const char *gui_buffer = \"\\" > gui.c
	sed 's/"/\\"/g' gui.glade | sed 's/.*/&\\n\\/' >> gui.c
	echo "\";" >> gui.c

cellrenderertk/cellrenderertk.o: cellrenderertk/cellrenderertk.c cellrenderertk/marshalers.h cellrenderertk/marshalers.c
	$(CC) -Wall `pkg-config gtk+-2.0 --cflags` cellrenderertk/cellrenderertk.c -o cellrenderertk/cellrenderertk.o -c

cellrenderertk/marshalers.o: cellrenderertk/marshalers.h cellrenderertk/marshalers.c
	$(CC) -Wall `pkg-config gtk+-2.0 --cflags` cellrenderertk/marshalers.c -o cellrenderertk/marshalers.o -c

cellrenderertk/marshalers.h: cellrenderertk/marshalers.list
	glib-genmarshal --prefix=_gtk_marshal --header cellrenderertk/marshalers.list > cellrenderertk/marshalers.h

cellrenderertk/marshalers.c: cellrenderertk/marshalers.list
	glib-genmarshal --prefix=_gtk_marshal --body cellrenderertk/marshalers.list > cellrenderertk/marshalers.c
