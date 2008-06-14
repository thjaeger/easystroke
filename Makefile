PREFIX   = /usr/local
DFLAGS   = #-ggdb #-pg
OFLAGS   = -Os
AOFLAGS  = -O3
CXXFLAGS = -Wall $(DFLAGS) `pkg-config gtkmm-2.4 libglademm-2.4 libglademm-2.4 --cflags`
LDFLAGS  = $(DFLAGS) -Lcellrenderertk
TARGETS  = easystroke

LIBS     = $(DFLAGS) -lcellrenderertk -lboost_serialization -lXtst `pkg-config gtkmm-2.4 libglademm-2.4 gthread-2.0 --cflags --libs`

LIBS_STATIC = $(DFLAGS) -lcellrenderertk -lXtst `pkg-config gtkmm-2.4 gthread-2.0 --cflags --libs` /usr/lib/libboost_serialization.a /usr/lib/libglademm-2.4.a -lglade-2.0 -lxml2
INCLUDES = 

BINARY   = easystroke

CCFILES  = $(wildcard *.cc)
OFILES   = $(patsubst %.cc,%.o,$(CCFILES)) dsimple.o gui.o
DEPFILES = $(wildcard *.Po)

all: $(TARGETS)

.PHONY: all clean $(BINARY)

clena:	clean

clean:
	$(MAKE) -C cellrenderertk clean
	$(RM) $(OFILES) $(BINARY) $(GENFILES) $(DEPFILES) gui.c gui.o

include $(DEPFILES)

easystroke: $(OFILES)
	$(MAKE) -C cellrenderertk
	$(CXX) $(LDFLAGS) -o $(BINARY) $(OFILES) $(LIBS)
	strip -s $(BINARY)

static: $(OFILES)
	$(MAKE) -C cellrenderertk
	$(CXX) $(LDFLAGS) -o $(BINARY) $(OFILES) $(LIBS_STATIC)
	strip -s $(BINARY)

stroke.o: stroke.cc
	$(CXX) $(CXXFLAGS) $(AOFLAGS) $(INCLUDES) -MT $@ -MMD -MP -MF $*.Po -o $@ -c $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(OFLAGS) $(INCLUDES) -MT $@ -MMD -MP -MF $*.Po -o $@ -c $<

gui.c: gui.glade
	echo "const char *gui_buffer = \"\\" > gui.c
	sed 's/"/\\"/g' gui.glade | sed 's/.*/&\\n\\/' >> gui.c
	echo "\";" >> gui.c
