#include "main.h"
#include "win.h"
#include "strokeaction.h"
#include "actiondb.h"
#include "prefdb.h"
#include "trace.h"
#include "copy.h"
#include "shape.h"
#include "grabber.h"

#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xrandr.h>
#include <X11/cursorfont.h>

#include <signal.h>
#include <fcntl.h>
#include <getopt.h>

bool gui = true;
bool experimental = false;
int verbosity = 0;

Display *dpy;

int fdw;

std::string config_dir;
Win *win;

void send(char c) {
	char cs[2] = {c, 0};
	write(fdw, cs, 1);
}

int (*oldHandler)(Display *, XErrorEvent *) = 0;

int xErrorHandler(Display *dpy2, XErrorEvent *e) {
	if (dpy != dpy2) {
		return oldHandler(dpy2, e);
	}

	char text[64];
	XGetErrorText(dpy, e->error_code, text, sizeof text);
	char msg[16];
	snprintf(msg, sizeof msg, "%d", e->request_code);
	char def[32];
	snprintf(def, sizeof def, "request_code=%d", e->request_code);
	char dbtext[128];
	XGetErrorDatabaseText(dpy, "easystroke", msg,
			def, dbtext, sizeof dbtext);
	std::cerr << "XError: " << text << ": " << dbtext << std::endl;
	return 0;

}

void run_gui() {
	win = new Win;
	Gtk::Main::run();
	delete win;
	send(P_QUIT);
}

class Timeout {
	int fd[2];
	bool active;
	int delay;
	void wait() {
		active = true;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(fd[0], &fds);
		int n = fd[0] + 1;
		while (1) {
			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = delay;
			if (select(n, &fds, 0, 0, &tv) == 0)
				break;
			char buffer[2];
			read(fd[0], buffer, 1);
		}
		active = false;
		send(P_TIMEOUT);
	}
public:
	Timeout() : active(false) {
		pipe(fd);
		fcntl(fd[0], F_SETFL, O_NONBLOCK);
	}
	void reset() {
		if (active)
			write(fd[1], "x", 1);
		else {
			delay = prefs().delay.get() * 1000;
			if (!delay)
				return;
			Glib::Thread::create(sigc::mem_fun(*this, &Timeout::wait), false);
		}

	}
};

void quit(int) {
	if (gui)
		win->quit();
	else
		send(P_QUIT);
}

Trace *init_trace() {
	switch(prefs().trace.get()) {
		case TraceNone:
			return new Trivial();
		case TraceShape:
			return new Shape();
		default:
			return new Copy();
	}
}


inline int sqr(int x) { return x*x; }

Trace::Point orig = {0, 0};
Window current = 0;
Grabber *grabber = 0;
bool ignore = false;
int ignore_button;
int press_button = 0;

class Main {
	enum GrabState { GS_IDLE, GS_STROKE, GS_ACTION };

	std::string parse_args_and_init_gtk(int argc, char **argv);
	void create_config_dir();
	void handle_stroke(int button, bool clear = true);
	char* next_event();
	void usage(char *me, bool good);

	Glib::Thread *gtk_thread;
	Gtk::Main *kit;
	Atom wm_delete;
	Trace *trace;
	RPreStroke cur;
	bool is_gesture;
	int fdr;
	int event_basep;
	bool randr;
	GrabState grab_state;
	bool xinput_works;
public:
	Main(int argc, char **argv);
	void run();
	~Main();
};

Main::Main(int argc, char **argv) : gtk_thread(0), kit(0), trace(0), is_gesture(false), grab_state(GS_IDLE), xinput_works(false) {
	if (0) {
		RStroke trefoil = Stroke::trefoil();
		trefoil->draw_svg("trefoil.svg");
		exit(EXIT_SUCCESS);
	}
	std::string display = parse_args_and_init_gtk(argc, argv);
	create_config_dir();

	dpy = XOpenDisplay(display.c_str());
	if (!dpy) {
		printf("Couldn't open display\n");
		exit(EXIT_FAILURE);
	}

	if (gui)
		gtk_thread = Glib::Thread::create(sigc::ptr_fun(&run_gui), true);
	actions().read();

	XSetWindowAttributes attr;
	attr.event_mask = SubstructureNotifyMask;
	XChangeWindowAttributes(dpy, ROOT, CWEventMask, &attr);

	wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", false);

	prefs().read();
	grabber = new Grabber;
	grabber->grab();

	int error_basep;
	randr = XRRQueryExtension(dpy, &event_basep, &error_basep);
	if (randr)
		XRRSelectInput(dpy, ROOT, RRScreenChangeNotifyMask);

	trace = init_trace();

	signal(SIGINT, &quit);

	int fds[2];
	pipe(fds);
	fdr = fds[0];
	fcntl(fdr, F_SETFL, O_NONBLOCK);
	fdw = fds[1];
}

void Main::usage(char *me, bool good) {
	printf("Usage: %s [OPTION]...\n", me);
	printf("  -c, --config-dir       Directory for config files\n");
	printf("      --display          X Server to contact\n");
	printf("  -e  --experimental     Start in experimental mode\n");
	printf("  -n, --no-gui           Don't start the gui\n");
	printf("  -v, --verbose          Increase verbosity level\n");
	exit(good ? EXIT_SUCCESS : EXIT_FAILURE);
}

std::string Main::parse_args_and_init_gtk(int argc, char **argv) {
	static struct option long_opts1[] = {
		{"display",1,0,'d'},
		{"help",0,0,'h'},
		{"no-gui",1,0,'n'},
		{0,0,0,0}
	};
	static struct option long_opts2[] = {
		{"config-dir",1,0,'c'},
		{"display",1,0,'d'},
		{"experimental",0,0,'e'},
		{"no-gui",0,0,'n'},
		{"verbose",0,0,'v'},
		{0,0,0,0}
	};
	std::string display;
	char opt;
	// parse --display here, before Gtk::Main(...) takes it away from us
	opterr = 0;
	while ((opt = getopt_long(argc, argv, "nh", long_opts1, 0)) != -1)
		switch (opt) {
			case 'd':
				display = optarg;
				break;
			case 'n':
				gui = false;
				break;
			case 'h':
				usage(argv[0], true);
		}
	optind = 1;
	opterr = 1;
	XInitThreads();
	Glib::thread_init();
	if (gui)
		kit = new Gtk::Main(argc, argv);
	oldHandler = XSetErrorHandler(xErrorHandler);

	while ((opt = getopt_long(argc, argv, "c:env", long_opts2, 0)) != -1) {
		switch (opt) {
			case 'c':
				config_dir = optarg;
				break;
			case 'e':
				experimental = true;
				break;
			case 'v':
				verbosity++;
				break;
			case 'd':
			case 'n':
				break;
			default:
				usage(argv[0], false);
		}
	}
	return display;
}

void Main::create_config_dir() {
	struct stat st;
	if (config_dir == "") {
		config_dir = getenv("HOME");
		config_dir += "/.easystroke";
	}
	if (lstat(config_dir.c_str(), &st) == -1) {
		if (mkdir(config_dir.c_str(), 0777) == -1) {
			printf("Error: Couldn't create configuration directory \"%s\"\n", config_dir.c_str());
			exit(EXIT_FAILURE);
		}
	} else {
		if (!S_ISDIR(st.st_mode)) {
			printf("Error: \"%s\" is not a directory\n", config_dir.c_str());
			exit(EXIT_FAILURE);
		}
	}
	config_dir += "/";
}

void Main::handle_stroke(int button, bool clear) {
	if (!is_gesture)
		cur->clear();
	if (!prefs().cds_stroke.get())
		cur->clear();
	RStroke s = Stroke::create(*cur, button);
	if (verbosity >= 3)
		s->print();
	if (gui) {
		if (!stroke_action()(s)) {
			Ranking ranking = actions().handle(s);
			if (ranking.id == -1)
				grabber->fake_button(grabber->button);
			win->stroke_push(ranking);
		}
		win->icon_push(s);
	} else {
		if (actions().handle(s).id == -1)
			grabber->fake_button(grabber->button);
	}
	if (clear) {
		if (press_button) {
			grabber->fake_button(press_button);
			press_button = 0;
		}
		clear_mods();
	}
}

char* Main::next_event() {
	static char buffer[2];
	int fdx = ConnectionNumber(dpy);
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fdr, &fds);
	FD_SET(fdx, &fds);
	int n = 1 + ((fdr>fdx) ? fdr : fdx);
	while (!XPending(dpy)) {
		select(n, &fds, 0, 0, 0);
		if (read(fdr, buffer, 1) > 0)
			return buffer;
	}
	return 0;
}

void Main::run() {
	int cur_size  = 0; // last cur size the timer saw
	Timeout timeout;
	guint emulated_button = 0;
	Time click_time = 0;
	if (verbosity >= 2)
		printf("Entering main loop...\n");
	while(1) {
		char *ret = next_event();
		if (ret) {
			if (*ret == P_QUIT)
				break;
			if (*ret == P_REGRAB)
				grabber->regrab();
			if (*ret == P_SUSPEND_GRAB)
				grabber->suspend();
			if (*ret == P_RESTORE_GRAB)
				grabber->restore();
			if (*ret == P_UPDATE_CURRENT) {
				prefs().write();
				grabber->update(current);
			}
			if (*ret == P_UPDATE_TRACE) {
				Trace *new_trace = init_trace();
				delete trace;
				trace = new_trace;
			}
			if (*ret == P_TIMEOUT) {
				if (cur->size() != cur_size)
					continue;
				handle_stroke(0);
			}
			if (*ret == P_IGNORE) {
				grabber->ignore(ignore_button);
				ignore = false;
			}
			continue;
		}
		XEvent ev;
		XNextEvent(dpy, &ev);

		switch(ev.type) {
			case MotionNotify:
				if (grab_state != GS_STROKE)
					break;
				Trace::Point p;
				p.x = ev.xmotion.x;
				p.y = ev.xmotion.y;
				cur->add(p.x, p.y);
				if (!is_gesture && ((sqr(p.x-orig.x)+sqr(p.y-orig.y)) > 100)) {
					is_gesture = true;
					trace->start(orig);
				}
				if (is_gesture)
					trace->draw(p);
				break;

			case ButtonPress:
				if (ignore) {
					ignore_button = ev.xbutton.button;
					// TODO: Is this necessary?
					send(P_IGNORE);
					break;
				}
				if (grab_state == GS_ACTION && xinput_works)
					break;

				if (ev.xbutton.button == grabber->button) {
					if (current)
						XSetInputFocus(dpy, current, RevertToParent, ev.xbutton.time);
					orig.x = ev.xbutton.x;
					orig.y = ev.xbutton.y;
					is_gesture = false;
					cur = PreStroke::create();
					grab_state = GS_STROKE;
					xinput_works = false;
					break;
				}
				if (grab_state == GS_IDLE)
					break;
//				if (stroke_click.special == SPECIAL_IGNORE)
//					break;

				if (grab_state == GS_STROKE)
					trace->end();

				if (grab_state != GS_ACTION) {
					if (gui && stroke_action()) {
						handle_stroke(ev.xbutton.button);
						grab_state = GS_IDLE;
						break;
					}
					if (xinput_works) {
						click_time = ev.xbutton.time;
						XTestFakeButtonEvent(dpy, ev.xbutton.button, False, CurrentTime);
						XTestFakeButtonEvent(dpy, grabber->button, False, CurrentTime);
						grabber->grab_xi();
						XGrabButton(dpy, AnyButton, AnyModifier, ROOT, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
					} else {
						printf("warning: Xinput extension not working correctly\n");
					}
				}
				grab_state = GS_ACTION;
				handle_stroke(ev.xbutton.button, false);
				if (!press_button)
					break;
				if (!xinput_works) {
					XTestFakeButtonEvent(dpy, ev.xbutton.button, False, CurrentTime);
					XTestFakeButtonEvent(dpy, grabber->button, False, CurrentTime);
					grabber->fake_button(press_button);
					press_button = 0;
					grab_state = GS_IDLE;
					clear_mods();
					break;
				}
				XUngrabButton(dpy, AnyButton, AnyModifier, ROOT);
				XTestFakeButtonEvent(dpy, press_button, True, CurrentTime);
				emulated_button = press_button;
				press_button = 0;
				XGrabButton(dpy, AnyButton, AnyModifier, ROOT, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
				break;

			case ButtonRelease:
				if (!xinput_works && grab_state == GS_ACTION && ev.xbutton.button == grabber->button) {
					clear_mods();
					break;
				}
				if (grab_state != GS_STROKE)
					break;
				trace->end();
				if (prefs().delay.get()) {
					cur_size = cur->size();
					timeout.reset();
				} else {
					handle_stroke(0);
				}
				grab_state = GS_IDLE;
				break;

			case ClientMessage:
				if((Atom)ev.xclient.data.l[0] == wm_delete)
					win->quit();
				break;

			case EnterNotify:
				if (ev.xcrossing.mode == NotifyGrab)
					break;
				if (ev.xcrossing.detail == NotifyInferior)
					break;
				current = ev.xcrossing.window;
				grabber->update(current);
				break;

			case CreateNotify:
				grabber->create(ev.xcreatewindow.window);
				break;

			case DestroyNotify:
				break;
			default:
				if (randr && ev.type == event_basep) {
					XRRUpdateConfiguration(&ev);
					Trace *new_trace = init_trace();
					delete trace;
					trace = new_trace;
				}
				if (grabber->xinput && grabber->is_button_down(ev.type)) {
					XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
					if (grab_state == GS_STROKE && bev->button == grabber->button) {
						xinput_works = true;
					}
					if (grab_state == GS_ACTION && xinput_works && bev->time != click_time) {
						XTestFakeButtonEvent(dpy, bev->button, False, CurrentTime);
						handle_stroke(bev->button, false);
						if (press_button) {
							if (emulated_button) {
								XTestFakeButtonEvent(dpy, emulated_button, False, CurrentTime);
								emulated_button = 0;
							}
							XUngrabButton(dpy, AnyButton, AnyModifier, ROOT);
							XTestFakeButtonEvent(dpy, press_button, True, CurrentTime);
							emulated_button = press_button;
							press_button = 0;
							XGrabButton(dpy, AnyButton, AnyModifier, ROOT, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
						}
					}
				}
				if (grabber->xinput && grabber->is_button_up(ev.type)) {
					XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
					if (grab_state == GS_ACTION && xinput_works) {
						if (bev->button == grabber->button) {
							clear_mods();
							XUngrabButton(dpy, AnyButton, AnyModifier, ROOT);
							grabber->grab_xi(false);
							grab_state = GS_IDLE;
						} else if (emulated_button) {
							XTestFakeButtonEvent(dpy, emulated_button, False, CurrentTime);
							emulated_button = 0;
						}
					}

				}
				break;
		}
	}
}

Main::~Main() {
	delete grabber;
	XCloseDisplay(dpy);
	if (gui)
		gtk_thread->join();
	delete kit;
	if (verbosity >= 2)
		printf("Exiting...\n");
}

int main(int argc, char **argv) {
	Main mn(argc, argv);
	mn.run();
}

bool SendKey::run() {

	if (xtest) {
		press();
		XTestFakeKeyEvent(dpy, code, true, 0);
		XTestFakeKeyEvent(dpy, code, false, 0);
		return true;
	}

	if (!current)
		return true;
	XKeyEvent ev;
	ev.type = KeyPress;	/* KeyPress or KeyRelease */
	ev.display = dpy;	/* Display the event was read from */
	ev.window = current;	/* ``event'' window it is reported relative to */
	ev.root = ROOT;		/* ROOT window that the event occurred on */
	ev.subwindow = 0;	/* child window */
	ev.time = CurrentTime;	/* milliseconds */
	ev.x = orig.x;		/* pointer x, y coordinates in event window */
	ev.y = orig.y;		/* pointer x, y coordinates in event window */
	ev.x_root = orig.x;	/* coordinates relative to root */
	ev.y_root = orig.y;	/* coordinates relative to root */
	ev.state = mods;	/* key or button mask */
	ev.keycode = code;	/* detail */
	ev.same_screen = true;	/* same screen flag */
	XSendEvent(dpy, current, True, KeyPressMask, (XEvent *)&ev);
	ev.type = KeyRelease;	/* KeyPress or KeyRelease */
	XSendEvent(dpy, current, True, KeyReleaseMask, (XEvent *)&ev);
	return true;
}

void Scroll::worker() {
#define GRAB (XGrabPointer(dpy, ROOT, False, PointerMotionMask|ButtonMotionMask|ButtonPressMask|ButtonReleaseMask, GrabModeAsync, GrabModeAsync, ROOT, cursor, CurrentTime) == GrabSuccess)
#define UNGRAB XUngrabPointer(dpy, CurrentTime);
	// We absolutely don't want to use the global dpy here, so it's probably
	// best to shadow it
	Display *dpy = XOpenDisplay(NULL);
	if (!dpy)
		return;

	press(); // NB: this uses the global display

	Cursor cursor = XCreateFontCursor(dpy, XC_double_arrow);
	GRAB;

	bool active = true;
	int lasty = -1;
	int pressed = 0;
	while (active) {
		XEvent ev;
		XNextEvent(dpy, &ev);
		switch (ev.type) {
			case MotionNotify: {
				int y = ev.xmotion.y;
				if (lasty == -1) {
					lasty = y;
					break;
				}
				int button = 0;
				if (y > lasty + 100)
					lasty = y;
				if (y > lasty + 10)
					button = 5;
				if (y < lasty - 100)
					lasty = y;
				if (y < lasty - 10)
					button = 4;
				if (button) {
					UNGRAB;
					if (pressed)
						XTestFakeButtonEvent(dpy, pressed, False, CurrentTime);
					XTestFakeButtonEvent(dpy, button, True, CurrentTime);
					XTestFakeButtonEvent(dpy, button, False, CurrentTime);
					lasty = y;
					while (!GRAB) usleep(10000);
					if (pressed)
						XTestFakeButtonEvent(dpy, pressed, True, CurrentTime);
				}
				break; }
			case ButtonPress:
				if (ev.xbutton.button < 4)
					pressed = ev.xbutton.button;
				break;
			case ButtonRelease:
				if (ev.xbutton.button < 4)
					active = false;
				break;
			default:
				printf("Unhandled event\n");
		}
	}
	UNGRAB;
	clear_mods();
	XFreeCursor(dpy, cursor);
	XCloseDisplay(dpy);
#undef GRAB
#undef UNGRAB
}


bool Scroll::run() {
	Glib::Thread::create(sigc::mem_fun(*this, &Scroll::worker), false);
	return true;
}

struct does_that_really_make_you_happy_stupid_compiler {
	guint mask;
	guint sym;
} modkeys[] = {
	{GDK_SHIFT_MASK, XK_Shift_L},
	{GDK_CONTROL_MASK, XK_Control_L},
	{GDK_MOD1_MASK, XK_Alt_L},
	{GDK_MOD2_MASK, 0},
	{GDK_MOD3_MASK, 0},
	{GDK_MOD4_MASK, 0},
	{GDK_MOD5_MASK, 0},
	{GDK_SUPER_MASK, XK_Super_L},
	{GDK_HYPER_MASK, XK_Hyper_L},
	{GDK_META_MASK, XK_Meta_L},
};
int n_modkeys = 10;

guint mod_state = 0;

void set_mod_state(int new_state) {
	for (int i = 0; i < n_modkeys; i++) {
		guint mask = modkeys[i].mask;
		if ((mod_state & mask) ^ (new_state & mask))
			XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, modkeys[i].sym), new_state & mask, 0);
	}
	mod_state = new_state;
}

void ButtonInfo::press() {
	set_mod_state(state);
}

void ModAction::press() {
	set_mod_state(mods);
}

void clear_mods() {
	set_mod_state(0);
}

bool Ignore::run() {
	grabber->grab_all(sigc::mem_fun(*this, &Ignore::press), sigc::ptr_fun(clear_mods));
	ignore = true;
	return true;
}

bool Button::run() {
	press();
	press_button = button;
	return true;
}
