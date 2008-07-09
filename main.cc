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
#include <X11/Xproto.h>

#include <signal.h>
#include <fcntl.h>
#include <getopt.h>

bool gui = true;
extern bool no_xi;
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
	if (verbosity == 0 && e->error_code == BadWindow && e->request_code == X_ChangeWindowAttributes)
		return 0;
	char text[64];
	XGetErrorText(dpy, e->error_code, text, sizeof text);
	char msg[16];
	snprintf(msg, sizeof msg, "%d", e->request_code);
	char def[32];
	snprintf(def, sizeof def, "request_code=%d", e->request_code);
	char dbtext[128];
	XGetErrorDatabaseText(dpy, "XRequest", msg,
			def, dbtext, sizeof dbtext);
	std::cerr << "XError: " << text << ": " << dbtext << std::endl;
	return 0;

}


void xi_warn() {
	static bool warned = false;
	if (warned)
		return;
	printf("warning: Xinput extension not working correctly\n");
	warned = true;
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

Window current = 0;
Grabber *grabber = 0;
bool ignore = false;
bool scroll = false;
guint press_button = 0;
Trace *trace = 0;

void handle_stroke(RStroke stroke, int button);

class Handler {
protected:
	Handler *child;
protected:
	virtual void grab() {}
	virtual void resume() { grab(); }
	virtual std::string name() = 0;
public:
	Handler *parent;
	Handler() : child(0), parent(0) {}
	Handler *top() {
		if (child)
			return child->top();
		else
			return this;
	}
	virtual void motion(int x, int y, Time t) {}
	virtual void press(guint b, int x, int y, Time t) {}
	virtual void release(guint b, int x, int y, Time t) {}
	virtual void press_repeated() {}
	void replace_child(Handler *c) {
		bool had_child = child;
		if (child)
			delete child;
		child = c;
		if (child)
			child->parent = this;
		if (verbosity >= 2) {
			std::string stack;
			for (Handler *h = child ? child : this; h; h=h->parent) {
				stack = h->name() + " " + stack;
			}
			std::cout << "New event handling stack: " << stack << std::endl;
		}
		if (child)
			child->init();
		if (!child && had_child)
			resume();
	}
	virtual void init() { grab(); }
	virtual bool idle() { return false; }
	virtual bool only_xi() { return false; }
	virtual ~Handler() {
		if (child)
			delete child;
	}
};

class IgnoreHandler : public Handler {
public:
	void grab() {
		grabber->grab(Grabber::ALL);
	}
	virtual void press(guint b, int x, int y, Time t) {
		grabber->ignore(b);
		parent->replace_child(0);
	}
	virtual ~IgnoreHandler() {
		clear_mods();
	}
	virtual std::string name() { return "Ignore"; }
};

class WaitForClickHandler : public Handler {
	guint button;
public:
	WaitForClickHandler(guint b) : button(b) {}
	virtual void press(guint b, int x, int y, Time t) {
		if (b == button)
			parent->replace_child(0);
	}
	virtual std::string name() { return "WaitForClick"; }
};

class ScrollHandler : public Handler {
	int lasty;
	guint pressed, pressed2;
public:
	ScrollHandler() : lasty(-255), pressed(0), pressed2(0) {
	}
	ScrollHandler(guint b, guint b2) : lasty(-255), pressed(b), pressed2(b2) {
	}
	virtual void init() {
		if (pressed2) {
			XTestFakeButtonEvent(dpy, pressed, False, CurrentTime);
			XTestFakeButtonEvent(dpy, pressed2, False, CurrentTime);
		}
		grabber->grab(Grabber::POINTER);
		if (pressed2) {
			XTestFakeButtonEvent(dpy, pressed2, True, CurrentTime);
			XTestFakeButtonEvent(dpy, pressed, True, CurrentTime);
			replace_child(new WaitForClickHandler(pressed2));
		}
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
			lasty = y;
			if (pressed)
				XTestFakeButtonEvent(dpy, pressed, False, CurrentTime);
			if (pressed2)
				XTestFakeButtonEvent(dpy, pressed2, False, CurrentTime);
			grabber->suspend();
			XTestFakeButtonEvent(dpy, button, True, CurrentTime);
			XTestFakeButtonEvent(dpy, button, False, CurrentTime);
			grabber->resume();
			if (pressed2)
				XTestFakeButtonEvent(dpy, pressed2, True, CurrentTime);
			if (pressed) {
				XTestFakeButtonEvent(dpy, pressed, True, CurrentTime);
				replace_child(new WaitForClickHandler(pressed));
			}
		}
	}
	virtual void press(guint b, int x, int y, Time t) {
		if (b != 4 && b != 5 && !pressed)
			pressed = b;
	}
	virtual void release(guint b, int x, int y, Time t) {
		if (b != pressed && b != pressed2)
			return;
		if (pressed2) {
			if (b == pressed) { // scroll button released, continue with Action
				XTestFakeButtonEvent(dpy, pressed, False, CurrentTime);
				XTestFakeButtonEvent(dpy, pressed2, False, CurrentTime);
				// Make sure event handling continues as usual
				XTestFakeButtonEvent(dpy, pressed2, True, CurrentTime);
				parent->replace_child(new WaitForClickHandler(pressed2));
			} else { // gesture button released, bail out
				XTestFakeButtonEvent(dpy, pressed, False, CurrentTime);
				XTestFakeButtonEvent(dpy, pressed2, False, CurrentTime);
				parent->parent->replace_child(0);
			}
		} else {
			clear_mods();
			parent->replace_child(0);
		}
	}
	virtual ~ScrollHandler() {
		clear_mods();
	}
	virtual std::string name() { return "Scroll"; }
};

bool xinput_pressed = false;

class ActionHandler : public Handler {
	RStroke stroke;
	guint button;

	void do_press() {
		handle_stroke(stroke, button);
		ignore = false;
		if (scroll) {
			scroll = false;
			replace_child(new ScrollHandler(button, grabber->button));
			return;
		}
		if (!press_button)
			return;
		XTestFakeButtonEvent(dpy, button, False, CurrentTime);
		XTestFakeButtonEvent(dpy, grabber->button, False, CurrentTime);
		grabber->fake_button(press_button);
		press_button = 0;
		clear_mods();
		parent->replace_child(0);
	}
public:
	ActionHandler(RStroke s, guint b) : stroke(s), button(b) {}

	virtual void init() {
		grabber->grab(Grabber::BUTTON);
		do_press();
	}

	virtual void press(guint b, int x, int y, Time t) {
		button = b;
		do_press();
	}

	virtual void resume() {
		grabber->grab(Grabber::BUTTON);
	}

	virtual void release(guint b, int x, int y, Time t) {
		if (b != grabber->button)
			return;
		clear_mods();
		parent->replace_child(0);
	}
	virtual ~ActionHandler() {
		clear_mods();
	}

	virtual std::string name() { return "Action"; }
};

class ActionXiHandler : public Handler {
	RStroke stroke;
	int emulated_button;

	guint button; // just for initialization
public:
	ActionXiHandler(RStroke s, guint b, Time t) : stroke(s), emulated_button(0), button(b) {
		XTestFakeButtonEvent(dpy, b, False, CurrentTime);
		XTestFakeButtonEvent(dpy, grabber->button, False, CurrentTime);
	}
	virtual void init() {
		handle_stroke(stroke, button);
		ignore = false;
		if (scroll) {
			scroll = false;
			replace_child(new ScrollHandler(button, grabber->button));
			return;
		}
		if (!press_button) {
			grabber->grab(Grabber::XI_ALL);
			return;
		}
		grabber->grab(Grabber::XI);
		XTestFakeButtonEvent(dpy, press_button, True, CurrentTime);
		grabber->grab(Grabber::XI_ALL);
		emulated_button = press_button;
		press_button = 0;
	}
	virtual void press(guint b, int x, int y, Time t) {
		XTestFakeButtonEvent(dpy, b, False, CurrentTime);
		handle_stroke(stroke, b);
		ignore = false;
		if (scroll) {
			scroll = false;
			button = b;
			replace_child(new ScrollHandler(b, grabber->button));
			return;
		}
		if (!press_button)
			return;
		if (emulated_button) {
			XTestFakeButtonEvent(dpy, emulated_button, False, CurrentTime);
			emulated_button = 0;
		}
		grabber->grab(Grabber::XI);
		XTestFakeButtonEvent(dpy, press_button, True, CurrentTime);
		grabber->grab(Grabber::XI_ALL);
		emulated_button = press_button;
		press_button = 0;
	}
	virtual void resume() {
		XTestFakeButtonEvent(dpy, button, False, CurrentTime);
		XTestFakeButtonEvent(dpy, grabber->button, False, CurrentTime);
		grabber->grab(Grabber::XI_ALL);
	}
	virtual void release(guint b, int x, int y, Time t) {
		if (emulated_button) {
			XTestFakeButtonEvent(dpy, emulated_button, False, CurrentTime);
			emulated_button = 0;
		}
		if (b == grabber->button)
			parent->replace_child(0);
	}
	virtual ~ActionXiHandler() {
		if (emulated_button) {
			XTestFakeButtonEvent(dpy, emulated_button, False, CurrentTime);
			emulated_button = 0;
		}
		clear_mods();
	}
	virtual bool only_xi() { return true; }
	virtual std::string name() { return "ActionXi"; }
};

Trace::Point orig;

Bool is_xi_press(Display *dpy, XEvent *ev, XPointer arg) {
	Time *t = (Time *)arg;
	if (!grabber->xinput)
		return false;
	if (!grabber->is_button_down(ev->type))
		return false;
	XDeviceButtonEvent* bev = (XDeviceButtonEvent *)ev;
	if (bev->button != grabber->button)
		return false;
	return bev->time == *t;
}

Atom _NET_ACTIVE_WINDOW, ATOM, _NET_WM_WINDOW_TYPE, _NET_WM_WINDOW_TYPE_DOCK;

void activate_window(Window w, Time t) {
#if 0
	if (_NET_ACTIVE_WINDOW == None) {
		XSetInputFocus(dpy, current, RevertToParent, t);
		return;
	}
	XClientMessageEvent ev;
	ev.type = ClientMessage;
	ev.window = w;
	ev.message_type = _NET_ACTIVE_WINDOW;
	ev.format = 32;
	ev.data.l[0] = 0; // 1 app, 2 pager
	ev.data.l[1] = t;
	ev.data.l[2] = 0;
	ev.data.l[3] = 0;
	ev.data.l[4] = 0;
	XSendEvent(dpy, ROOT, False, SubstructureNotifyMask | SubstructureRedirectMask, (XEvent *)&ev);
#else
	Atom window_type = None;
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop_return = NULL;

	if (XGetWindowProperty(dpy, w, _NET_WM_WINDOW_TYPE, 0, sizeof(Atom), False, ATOM, &actual_type, &actual_format, 
				&nitems, &bytes_after, &prop_return) == Success && prop_return) {
		window_type = *(Atom *)prop_return;
		XFree(prop_return);
	}

	if (window_type != _NET_WM_WINDOW_TYPE_DOCK)
		XSetInputFocus(dpy, current, RevertToParent, t);
#endif
}

class StrokeHandler : public Handler {
	RPreStroke cur;
	bool is_gesture;
	float speed;
	Time last_t;
	int last_x, last_y;
	bool repeated;
	bool have_xi;

	RStroke finish(guint b) {
		trace->end();
		XFlush(dpy);
		if (have_xi)
			XAllowEvents(dpy, AsyncPointer, CurrentTime);
		if (!is_gesture)
			cur->clear();
		if (b && prefs().advanced_ignore.get())
			cur->clear();
		return Stroke::create(*cur, b);
	}

	bool calc_speed(int x, int y, Time t) {
		if (!have_xi)
			return false;
		int dt = t - last_t;
		float c = exp(dt/-250.0);
		if (dt) {
			float dist = hypot(x-last_x, y-last_y);
			speed = c * speed + (1-c) * dist/dt;
		} else {
			speed = c * speed;
		}
		last_x = x;
		last_y = y;
		last_t = t;

		if (speed >= 0.04)
			return false;
		trace->end();
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		parent->replace_child(0);
		return true;
	}
protected:
	virtual void press_repeated() {
		repeated = true;
	}
	virtual void motion(int x, int y, Time t) {
		if (!repeated && xinput_pressed && !prefs().ignore_grab.get()) {
			parent->replace_child(0);
			return;
		}
		cur->add(x,y,t);
		if (!is_gesture && hypot(x-orig.x, y-orig.y) > prefs().radius.get()) {
			is_gesture = true;
			bool first = true;
			for (std::vector<Stroke::Point>::iterator i = cur->points.begin(); i != cur->points.end(); i++) {
				Trace::Point p;
				p.x = i->x;
				p.y = i->y;
				if (first) {
					trace->start(p);
					first = false;
				} else {
					trace->draw(p);
				}
			}
		} else if (is_gesture) {
			Trace::Point p;
			p.x = x;
			p.y = y;
			trace->draw(p);
		}
		calc_speed(x,y,t);
	}

	virtual void press(guint b, int x, int y, Time t) {
		if (b == grabber->button)
			return;
		if (calc_speed(x,y,t))
			return;
		RStroke s = finish(b);

		if (gui && stroke_action()) {
			handle_stroke(s, b);
			parent->replace_child(0);
			return;
		}

		if (xinput_pressed) {
			parent->replace_child(new ActionXiHandler(s, b, t));
		} else {
			xi_warn();
			parent->replace_child(new ActionHandler(s, b));
		}
	}

	virtual void release(guint b, int x, int y, Time t) {
		if (calc_speed(x,y,t))
			return;
		RStroke s = finish(b);

		handle_stroke(s, 0);
		if (ignore) {
			ignore = false;
			parent->replace_child(new IgnoreHandler);
			return;
		}
		if (scroll) {
			scroll = false;
			parent->replace_child(new ScrollHandler);
			return;
		}
		if (press_button && !(!repeated && xinput_pressed && press_button == grabber->button)) {
			grabber->fake_button(press_button);
			press_button = 0;
		}
		clear_mods();
		parent->replace_child(0);
	}
public:
	StrokeHandler(int x, int y, Time t) : is_gesture(false), speed(0.1), last_t(t), last_x(x), last_y(y), 
	repeated(false), have_xi(false) {
		orig.x = x; orig.y = y;
		cur = PreStroke::create();
		cur->add(x,y,t);
		if (xinput_pressed)
			have_xi = true;
		XEvent ev;
		if (!have_xi) {
			have_xi = XCheckIfEvent(dpy, &ev, &is_xi_press, (XPointer)&t);
			if (have_xi)
				repeated = true;
		}
		if (have_xi) {
			xinput_pressed = true;
		} else {
			XAllowEvents(dpy, AsyncPointer, CurrentTime);
			xi_warn();
		}
	}
	~StrokeHandler() {
		trace->end();
		if (have_xi)
			XAllowEvents(dpy, AsyncPointer, CurrentTime);
	}
	virtual std::string name() { return "Stroke"; }
};

class IdleHandler : public Handler {
protected:
	virtual void init() {
		XGrabKey(dpy, XKeysymToKeycode(dpy,XK_Escape), AnyModifier, ROOT, True, GrabModeAsync, GrabModeSync);
		grab();
	}
	virtual void press(guint b, int x, int y, Time t) {
		if (b != grabber->button)
			return;
		if (current)
			activate_window(current, t);
		replace_child(new StrokeHandler(x, y, t));
	}
	virtual void grab() {
		grabber->grab(Grabber::BUTTON);
	}
	virtual void resume() {
		grab();
	}
public:
	virtual bool idle() { return true; }
	virtual std::string name() { return "Idle"; }
	virtual ~IdleHandler() {
		XUngrabKey(dpy, XKeysymToKeycode(dpy,XK_Escape), AnyModifier, ROOT);
	}
};

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
	grabber->grab(Grabber::BUTTON);

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

	_NET_ACTIVE_WINDOW = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", True);
	ATOM = XInternAtom(dpy, "ATOM", True);
	_NET_WM_WINDOW_TYPE = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", True);
	_NET_WM_WINDOW_TYPE_DOCK = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", True);
}

void Main::usage(char *me, bool good) {
	printf("Usage: %s [OPTION]...\n", me);
	printf("  -c, --config-dir       Directory for config files\n");
	printf("      --display          X Server to contact\n");
	printf("  -x  --no-xi            Don't use the Xinput extension\n");
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
		{"no-xi",1,0,'x'},
		{0,0,0,0}
	};
	static struct option long_opts2[] = {
		{"config-dir",1,0,'c'},
		{"display",1,0,'d'},
		{"experimental",0,0,'e'},
		{"no-gui",0,0,'n'},
		{"no-xi",0,0,'x'},
		{"verbose",0,0,'v'},
		{0,0,0,0}
	};
	std::string display;
	char opt;
	// parse --display here, before Gtk::Main(...) takes it away from us
	opterr = 0;
	while ((opt = getopt_long(argc, argv, "nhx", long_opts1, 0)) != -1)
		switch (opt) {
			case 'd':
				display = optarg;
				break;
			case 'n':
				gui = false;
				break;
			case 'h':
				usage(argv[0], true);
				break;
			case 'x':
				no_xi = true;
				break;
		}
	optind = 1;
	opterr = 1;
	XInitThreads();
	Glib::thread_init();
	if (gui)
		kit = new Gtk::Main(argc, argv);
	oldHandler = XSetErrorHandler(xErrorHandler);

	while ((opt = getopt_long(argc, argv, "c:envx", long_opts2, 0)) != -1) {
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
			case 'x':
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
				press_button = grabber->button;
			win->stroke_push(ranking);
		}
		win->icon_push(s);
	} else {
		if (actions().handle(s).id == -1)
			press_button = grabber->button;
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
	Handler *handler = new IdleHandler;
	handler->init();
	bool alive = true;

	Time last_time = 0;
	int last_type = 0;
	guint last_button = 0;
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
					//handler->cancel();
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
				grabber->resume();
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

		try {
		switch(ev.type) {
			case MotionNotify:
				if (handler->top()->only_xi())
					break;
				if (last_type == MotionNotify && last_time == ev.xmotion.time) {
					break;
				}
				handler->top()->motion(ev.xmotion.x, ev.xmotion.y, ev.xmotion.time);
				last_type = MotionNotify;
				last_time = ev.xmotion.time;
				break;

			case ButtonPress:
				if (handler->top()->only_xi())
					break;
				if (last_type == ButtonPress && last_time == ev.xbutton.time && last_button == ev.xbutton.button) {
					handler->top()->press_repeated();
					break;
				}
				handler->top()->press(ev.xbutton.button, ev.xbutton.x, ev.xbutton.y, ev.xbutton.time);
				last_type = ButtonPress;
				last_time = ev.xbutton.time;
				last_button = ev.xbutton.button;
				break;

			case ButtonRelease:
				if (handler->top()->only_xi())
					break;
				if (last_type == ButtonRelease && last_time == ev.xbutton.time && last_button == ev.xbutton.button)
					break;
				handler->top()->release(ev.xbutton.button, ev.xbutton.x, ev.xbutton.y, ev.xbutton.time);
				if (ev.xbutton.button == grabber->button)
					xinput_pressed = false;
				last_type = ButtonRelease;
				last_time = ev.xbutton.time;
				last_button = ev.xbutton.button;
				break;

			case KeyPress:
				if (ev.xkey.keycode != XKeysymToKeycode(dpy, XK_Escape))
					break;
				XAllowEvents(dpy, ReplayKeyboard, CurrentTime);
				if (handler->top()->idle())
					break;
				printf("Escape pressed: Resetting...\n");
				handler->replace_child(0);
				for (int i = 1; i <= 9; i++)
					XTestFakeButtonEvent(dpy, i, False, CurrentTime);
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
					if (bev->button == grabber->button)
						xinput_pressed = true;
					if (last_type == ButtonPress && last_time == bev->time && last_button == bev->button) {
						handler->top()->press_repeated();
						break;
					}
					handler->top()->press(bev->button, bev->x, bev->y, bev->time);
					last_type = ButtonPress;
					last_time = bev->time;
					last_button = bev->button;
				}
				if (grabber->xinput && grabber->is_button_up(ev.type)) {
					XDeviceButtonEvent* bev = (XDeviceButtonEvent *)&ev;
					if (last_type == ButtonRelease && last_time == bev->time && last_button == bev->button)
						break;
					handler->top()->release(bev->button, bev->x, bev->y, bev->time);
					if (bev->button == grabber->button)
						xinput_pressed = false;
					last_type = ButtonRelease;
					last_time = bev->time;
					last_button = bev->button;
				}
				if (grabber->xinput && grabber->is_motion(ev.type)) {
					XDeviceMotionEvent* mev = (XDeviceMotionEvent *)&ev;
					if (last_type == MotionNotify && last_time == mev->time)
						break;
					handler->top()->motion(mev->x, mev->y, mev->time);
					last_type = MotionNotify;
					last_time = mev->time;
				}
				break;
		}
		} catch (GrabFailedException) {
			printf("Error: A grab failed.  Resetting...\n");
			handler->replace_child(0);
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
	ev.time = CurrentTime;	/* milliseconds */
	XTranslateCoordinates(dpy, ROOT, current, orig.x, orig.y, &ev.x, &ev.y, &ev.subwindow);
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

bool Button::run() {
	if (1) {
		press();
		press_button = button;
		return true;
	}
	if (!current)
		return true;
	// Doesn't work!
	XButtonEvent ev;
	ev.type = ButtonPress;  /* ButtonPress or ButtonRelease */
	ev.display = dpy;	/* Display the event was read from */
	ev.window = current;	/* ``event'' window it is reported relative to */
	ev.root = ROOT;		/* ROOT window that the event occurred on */
	ev.time = CurrentTime;	/* milliseconds */
	XTranslateCoordinates(dpy, ROOT, current, orig.x, orig.y, &ev.x, &ev.y, &ev.subwindow);
	ev.x_root = orig.x;	/* coordinates relative to root */
	ev.y_root = orig.y;	/* coordinates relative to root */
	ev.state = mods;	/* key or button mask */
	ev.button = button;     /* detail */
	ev.same_screen = true;	/* same screen flag */

	XSendEvent(dpy, current, True, ButtonPressMask, (XEvent *)&ev);
	ev.type = ButtonRelease;/* ButtonPress or ButtonRelease */
	XSendEvent(dpy, current, True, ButtonReleaseMask, (XEvent *)&ev);
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
