PREFIX   = /usr/local
DFLAGS   = #-ggdb #-pg
OFLAGS   = -Os
AOFLAGS  = -O3
CXXFLAGS = -Wall $(DFLAGS) `pkg-config gtkmm-2.4 libglademm-2.4 --cflags`
TARGETS  = easystroke

LIBS     = $(DFLAGS) -lboost_serialization -lXtst `pkg-config gtkmm-2.4 libglademm-2.4 gthread-2.0 --cflags --libs`
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
	$(CXX) $(LDFLAGS) -o $(BINARY) $(OFILES) -Lcellrenderertk -lcellrenderertk $(LIBS)

stroke.o: stroke.cc
	$(CXX) $(CXXFLAGS) $(AOFLAGS) $(INCLUDES) -MT $@ -MMD -MP -MF $*.Po -o $@ -c $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(OFLAGS) $(INCLUDES) -MT $@ -MMD -MP -MF $*.Po -o $@ -c $<

gui.c: gui.glade
	echo "const char *gui_buffer = \"\\" > gui.c
	sed 's/"/\\"/g' gui.glade | sed 's/.*/&\\n\\/' >> gui.c
	echo "\";" >> gui.c
