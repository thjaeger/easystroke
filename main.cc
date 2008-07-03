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
	gui = false;
	delete win;
	send(P_QUIT);
}

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

Window current = 0;
Grabber *grabber = 0;
bool ignore = false;
bool scroll = false;
int press_button = 0;
Trace *trace = 0;

void handle_stroke(RStroke stroke, int button);

class Handler {
public:
	virtual void motion(int x, int y, Time t) {}
	virtual Handler *press(guint b, int x, int y, Time t) { return 0; }
	virtual Handler *release(guint b, int x, int y) { return 0; }
	virtual Handler *xi_press(guint b, int x, int y, Time t) { return 0; }
	virtual Handler *xi_release(guint b, int x, int y) { return 0;}
	virtual bool idle() { return false; }
	virtual void cancel() {}
};

class IdleHandler : public Handler {
public:
	IdleHandler() {
		if (verbosity >= 2) printf("Switching to Idle mode\n");
	}
	virtual Handler *press(guint b, int x, int y, Time t);
	virtual bool idle() { return true; }
};

Trace::Point orig; // TODO

class StrokeHandler : public Handler {
	RPreStroke cur;
	bool is_gesture;
	bool xinput_works;
	int orig_x, orig_y;

	RStroke finish(guint b);
public:
	StrokeHandler(int x, int y) : is_gesture(false), xinput_works(false), orig_x(x), orig_y(y) {
		if (verbosity >= 2) printf("Switching to Stroke mode\n");
		cur = PreStroke::create();
		cur->add(orig_x, orig_y);
	}
	virtual void motion(int x, int y, Time t);

	virtual Handler *xi_press(guint b, int x, int y, Time t) {
		if (b == grabber->button)
			xinput_works = true;
		return 0;
	}

	virtual Handler *press(guint b, int x, int y, Time t);
	virtual Handler *release(guint b, int x, int y);
};

class ActionHandler : public Handler {
	RStroke stroke;

	ActionHandler(RStroke s) : stroke(s) {}
	static Handler *do_press(RStroke s, guint b);
public:
	static Handler *create(RStroke s, guint b) {
		if (verbosity >= 2) printf("Switching to Action mode\n");
		Handler *h = do_press(s, b);
		if (h)
			return h;
		else
			return new ActionHandler(s);
	}

	virtual Handler *press(guint b, int x, int y, Time t) {
		return do_press(stroke, b);
	}

	virtual Handler *release(guint b, int x, int y) {
		if (b != grabber->button)
			return 0;
		clear_mods();
		return new IdleHandler;
	}
	virtual void cancel() {
		clear_mods();
	}

};

class ActionXiHandler : public Handler {
	RStroke stroke;
	Time click_time;
	int emulated_button;
public:
	ActionXiHandler(RStroke s, guint b, Time t);
	virtual Handler *xi_press(guint b, int x, int y, Time t);
	virtual Handler *xi_release(guint b, int x, int y);
	virtual void cancel();
};

class IgnoreHandler : public Handler {
public:
	IgnoreHandler() {
		if (verbosity >= 2) printf("Switching to Ignore mode\n");
		grabber->grab_all();
	}
	virtual Handler *press(guint b, int x, int y, Time t) {
		grabber->ignore(b);
		return new IdleHandler;
	}
	virtual void cancel() {
		clear_mods();
		grabber->grab_all(false);
	}
};

class ScrollHandler : public Handler {
	int lasty;
	guint pressed;
	int ignore_release;
public:
	ScrollHandler() : lasty(-255), pressed(0), ignore_release(0) {
		if (verbosity >= 2) printf("Switching to Scroll mode\n");
		grabber->grab_pointer();
	}
	ScrollHandler(guint b) : lasty(-255), pressed(b), ignore_release(1) {
		XTestFakeButtonEvent(dpy, b, False, CurrentTime);
		grabber->grab_pointer();
		XTestFakeButtonEvent(dpy, b, True, CurrentTime);
	}
	virtual void motion(int x, int y, Time t) {
		if (lasty == -1) {
			lasty = y;
			return;
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
			grabber->suspend();
			if (pressed)
				XTestFakeButtonEvent(dpy, pressed, False, CurrentTime);
			XTestFakeButtonEvent(dpy, button, True, CurrentTime);
			XTestFakeButtonEvent(dpy, button, False, CurrentTime);
			lasty = y;
			grabber->restore();
			if (pressed)
				XTestFakeButtonEvent(dpy, pressed, True, CurrentTime);
		}
	}
	virtual Handler *press(guint b, int x, int y, Time t) {
		if (b != 4 && b != 5)
			pressed = b;
		return 0;
	}
	virtual Handler *release(guint b, int x, int y) {
		if (b != pressed)
			return 0;
		if (ignore_release) {
			ignore_release--;
			return 0;
		}
		clear_mods();
		grabber->grab_pointer(false);
		return new IdleHandler;
	}
	virtual void cancel() {
		clear_mods();
		grabber->grab_pointer(false);
	}
};


Handler *IdleHandler::press(guint b, int x, int y, Time t) {
	if (b != grabber->button)
		return 0;
	if (current)
		XSetInputFocus(dpy, current, RevertToParent, t);
	return new StrokeHandler(x, y);
}

RStroke StrokeHandler::finish(guint b) {
	trace->end();

	if (!is_gesture)
		cur->clear();
	if (b && prefs().advanced_ignore.get())
		cur->clear();
	return Stroke::create(*cur, b);
}

void StrokeHandler::motion(int x, int y, Time t) {
	Trace::Point p;
	cur->add(x, y);
	if (!is_gesture && ((sqr(x-orig_x)+sqr(y-orig_y)) > sqr(prefs().radius.get()))) {
		is_gesture = true;
		orig.x = orig_x;
		orig.y = orig_y;
		trace->start(orig);
	}
	p.x = x;
	p.y = y;
	if (is_gesture)
		trace->draw(p);
}

Handler *StrokeHandler::press(guint b, int x, int y, Time t) {
	if (b == grabber->button)
		return 0;
	RStroke s = finish(b);

	if (gui && stroke_action()) {
		handle_stroke(s, b);
		return new IdleHandler;
	}

	if (xinput_works) {
		return new ActionXiHandler(s, b, t);
	} else {
		printf("warning: Xinput extension not working correctly\n");
		return ActionHandler::create(s, b);
	}
}

Handler *StrokeHandler::release(guint b, int x, int y) {
	RStroke s = finish(b);

	handle_stroke(s, 0);
	if (ignore) {
		ignore = false;
		return new IgnoreHandler;
	}
	if (scroll) {
		scroll = false;
		return new ScrollHandler;
	}
	if (press_button) {
		grabber->fake_button(press_button);
		press_button = 0;
	}
	clear_mods();
	return new IdleHandler;
}

Handler *ActionHandler::do_press(RStroke s, guint b) {
	handle_stroke(s, b);
	ignore = false;
	if (scroll) {
		scroll = false;
		XTestFakeButtonEvent(dpy, grabber->button, False, CurrentTime);
		return new ScrollHandler(b);
	}
	if (!press_button)
		return 0;
	XTestFakeButtonEvent(dpy, b, False, CurrentTime);
	XTestFakeButtonEvent(dpy, grabber->button, False, CurrentTime);
	grabber->fake_button(press_button);
	press_button = 0;
	clear_mods();
	return new IdleHandler;
}

ActionXiHandler::ActionXiHandler(RStroke s, guint b, Time t) : stroke(s), click_time(t), emulated_button(0) {
	if (verbosity >= 2) printf("Switching to ActionXi mode\n");
	XTestFakeButtonEvent(dpy, b, False, CurrentTime);
	XTestFakeButtonEvent(dpy, grabber->button, False, CurrentTime);
	grabber->grab_xi();
	handle_stroke(stroke, b);
	ignore = false;
	scroll = false; // TODO
	if (!press_button) {
		XGrabButton(dpy, AnyButton, AnyModifier, ROOT, True, ButtonPressMask,
				GrabModeAsync, GrabModeAsync, None, None);
		return;
	}
	XTestFakeButtonEvent(dpy, press_button, True, CurrentTime);
	emulated_button = press_button;
	press_button = 0;
	XGrabButton(dpy, AnyButton, AnyModifier, ROOT, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
}

Handler *ActionXiHandler::xi_press(guint b, int x, int y, Time t) {
	if (t == click_time)
		return 0;
	XTestFakeButtonEvent(dpy, b, False, CurrentTime);
	handle_stroke(stroke, b);
	ignore = false;
	scroll = false; // TODO
	if (!press_button)
		return 0;
	if (emulated_button) {
		XTestFakeButtonEvent(dpy, emulated_button, False, CurrentTime);
		emulated_button = 0;
	}
	XUngrabButton(dpy, AnyButton, AnyModifier, ROOT);
	XTestFakeButtonEvent(dpy, press_button, True, CurrentTime);
	emulated_button = press_button;
	press_button = 0;
	XGrabButton(dpy, AnyButton, AnyModifier, ROOT, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
	return 0;
}

Handler *ActionXiHandler::xi_release(guint b, int x, int y) {
	if (emulated_button) {
		XTestFakeButtonEvent(dpy, emulated_button, False, CurrentTime);
		emulated_button = 0;
	}
	if (b == grabber->button) {
		clear_mods();
		XUngrabButton(dpy, AnyButton, AnyModifier, ROOT);
		grabber->grab_xi(false);
		return new IdleHandler;
	}
	return 0;
}

void ActionXiHandler::cancel() {
	if (emulated_button) {
		XTestFakeButtonEvent(dpy, emulated_button, False, CurrentTime);
		emulated_button = 0;
	}
	clear_mods();
	XUngrabButton(dpy, AnyButton, AnyModifier, ROOT);
	grabber->grab_xi(false);
}

Handler *handler;

class Main {
	std::string parse_args_and_init_gtk(int argc, char **argv);
	void create_config_dir();
	char* next_event();
	void usage(char *me, bool good);

	Glib::Thread *gtk_thread;
	Gtk::Main *kit;
	int fdr;
	int event_basep;
	bool randr;


public:
	Main(int argc, char **argv);
	void run();
	~Main();
};

Main::Main(int argc, char **argv) : gtk_thread(0), kit(0) {
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

void handle_stroke(RStroke s, int button) {
	s->button = button;
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
	handler = new IdleHandler;
	bool alive = true;
	if (verbosity >= 2)
		printf("Entering main loop...\n");
	while (alive || !handler->idle()) {
		char *ret = next_event();
		if (ret) {
			if (*ret == P_QUIT) {
				if (alive) {
					alive = false;
				} else {
					printf("Forcing easystroke to quit\n");
					handler->cancel();
					delete handler;
					handler = new IdleHandler;
				}
				continue;
			}
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
			continue;
		}
		XEvent ev;
		XNextEvent(dpy, &ev);

		Handler *nh;

		switch(ev.type) {
			case MotionNotify:
				handler->motion(ev.xmotion.x, ev.xmotion.y, ev.xmotion.time);
				break;

			case ButtonPress:
				nh = handler->press(ev.xbutton.button, ev.xbutton.x, ev.xbutton.y, ev.xbutton.time);
				if (nh) {
					delete handler;
					handler = nh;
				}
				break;

			case ButtonRelease:
				nh = handler->release(ev.xbutton.button, ev.xbutton.x, ev.xbutton.y);
				if (nh) {
					delete handler;
					handler = nh;
				}
				break;

			case ClientMessage:
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
					nh = handler->xi_press(bev->button, bev->x, bev->y, bev->time);
					if (nh) {
						delete handler;
						handler = nh;
					}
				}
				if (grabber->xinput && grabber->is_button_up(ev.type)) {
					XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
					nh = handler->xi_release(bev->button, bev->x, bev->y);
					if (nh) {
						delete handler;
						handler = nh;
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

bool Scroll::run() {
	press();
	scroll = true;
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
	press();
	ignore = true;
	return true;
}

bool Button::run() {
	press();
	press_button = button;
	return true;
}
