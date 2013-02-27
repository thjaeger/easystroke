/*
 * Copyright (c) 2008-2009, Thomas Jaeger <ThJaeger@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "win.h"
#include "main.h"
#include "shape.h"
#include "prefs.h"
#include "actiondb.h"
#include "prefdb.h"
#include "trace.h"
#include "annotate.h"
#include "fire.h"
#include "water.h"
#include "composite.h"
#include "grabber.h"
#include "handler.h"

#include <glibmm/i18n.h>

#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xfixes.h>

#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <getopt.h>

extern Source<bool> disabled;

bool experimental = false;
int verbosity = 0;
const char *prefs_versions[] = { "-0.5.5", "-0.4.1", "-0.4.0", "", NULL };
const char *actions_versions[] = { "-0.5.6", "-0.4.1", "-0.4.0", "", NULL };
Source<Window> current_app_window(None);
std::string config_dir;
Win *win = NULL;
Display *dpy;
Window ROOT;

boost::shared_ptr<Trace> trace;

static ActionDBWatcher *action_watcher = 0;

static Trace *trace_composite() {
	try {
		return new Composite();
	} catch (std::exception &e) {
		if (verbosity >= 1)
			printf("Falling back to Shape method: %s\n", e.what());
		return new Shape();
	}
}

static Trace *init_trace() {
	try {
		switch(prefs.trace.get()) {
			case TraceNone:
				return new Trivial();
			case TraceShape:
				return new Shape();
			case TraceAnnotate:
				return new Annotate();
			case TraceFire:
				return new Fire();
			case TraceWater:
				return new Water();
			default:
				return trace_composite();
		}
	} catch (DBusException &e) {
		printf(_("Error: %s\n"), e.what());
		return trace_composite();
	}

}

class OSD : public Gtk::Window {
	static std::list<OSD *> osd_stack;
	int w;
public:
	OSD(Glib::ustring txt) : Gtk::Window(Gtk::WINDOW_POPUP) {
		osd_stack.push_back(this);
		set_accept_focus(false);
		set_border_width(15);
		WIDGET(Gtk::Label, label, "<big><b>" + txt + "</b></big>");
		label.set_use_markup();
		label.override_color(Gdk::RGBA("White"), Gtk::STATE_FLAG_NORMAL);
		override_background_color(Gdk::RGBA("RoyalBlue3"), Gtk::STATE_FLAG_NORMAL);
		set_opacity(0.75);
		add(label);
		label.show();
		int h;
		get_size(w,h);
		do_move();
		show();
		get_window()->input_shape_combine_region(Cairo::Region::create(), 0, 0);
	}
	static void do_move() {
		int left = gdk_screen_width() - 10;
		for (std::list<OSD *>::iterator i = osd_stack.begin(); i != osd_stack.end(); i++) {
			left -= (*i)->w + 30;
			(*i)->move(left, 40);
		}
	}
	virtual ~OSD() {
		osd_stack.remove(this);
		do_move();
	}
};

std::list<OSD *> OSD::osd_stack;

void Trace::start(Trace::Point p) {
	last = p;
	active = true;
	XFixesHideCursor(dpy, ROOT);
	start_();
}

void Trace::end() {
	if (!active)
		return;
	active = false;
	XFixesShowCursor(dpy, ROOT);
	end_();
}

void icon_warning() {
	for (ActionDB::const_iterator i = actions.begin(); i != actions.end(); i++) {
		Misc *m = dynamic_cast<Misc *>(i->second.action.get());
		if (m && m->type == Misc::SHOWHIDE)
			return;
	}
	if (!win)
		return;

	Gtk::MessageDialog *md;
	widgets->get_widget("dialog_icon", md);
	md->set_message(_("Tray icon disabled"));
	md->set_secondary_text(_("To bring the configuration dialog up again, you should define an action of type Misc...Show/Hide."));
	md->run();
	md->hide();
}

void quit() {
	static bool dead = false;
	if (dead)
		xstate->bail_out();
	dead = true;
	win->hide();
	xstate->queue(sigc::ptr_fun(&Gtk::Main::quit));
}

class App : public Gtk::Application, Base {
public:
	App(int& argc, char**& argv, const Glib::ustring& application_id, Gio::ApplicationFlags flags=Gio::APPLICATION_FLAGS_NONE) :
		Gtk::Application(argc, argv, application_id, flags), remote(false) {}
	~App();
protected:
	virtual void on_startup();
	virtual void on_activate();
private:
	virtual bool local_command_line_vfunc (char**& arguments, int& exit_status);
	int on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &);
	void run_by_name(const char *str, const Glib::RefPtr<Gio::ApplicationCommandLine> &cmd_line);

	void usage(const char *me);
	void version();

	void create_config_dir();

	void on_about(const Glib::VariantBase &) { win->show_about(); }
	void on_quit(const Glib::VariantBase &) { quit(); }

	virtual void notify() {
		g_simple_action_set_state(enabled, g_variant_new("b", !disabled.get()));
	}

	bool remote;
	GSimpleAction *enabled;
};

class ReloadTrace : public Timeout {
	void timeout() {
		if (verbosity >= 2)
			printf("Reloading gesture display\n");
		xstate->queue(sigc::mem_fun(*this, &ReloadTrace::reload));
	}
	void reload() { trace.reset(init_trace()); }
} reload_trace;

static void schedule_reload_trace() { reload_trace.set_timeout(1000); }

extern const char *gui_buffer;

bool App::local_command_line_vfunc (char**& arg, int& exit_status) {
	int i = 1;
	while (arg[i] && arg[i][0] == '-') {
		if (arg[i][1] == '-') {
			if (!strcmp(arg[i], "--experimental")) {
				experimental = true;
			} else if (!strcmp(arg[i], "--verbose")) {
				verbosity++;
			} else if (!strcmp(arg[i], "--help")) {
				usage(arg[0]);
				exit_status = EXIT_SUCCESS;
				return true;
			} else if (!strcmp(arg[i], "--version")) {
				version();
				exit_status = EXIT_SUCCESS;
				return true;
			} else if (!strcmp(arg[i], "--config-dir")) {
				if (!arg[++i]) {
					printf("Error: Option --config-dir requires an argument.\n");
					exit_status = EXIT_FAILURE;
					return true;
				}
				config_dir = arg[i];
			} else {
				printf("Error: Unknown option %s\n", arg[i]);
				exit_status = EXIT_FAILURE;
				return true;
			}
		} else {
			for (int j = 1; arg[i][j]; j++)
				switch (arg[i][j]) {
					case 'c':
						if (arg[i][j+1] || !arg[i+1]) {
							printf("Error: Option -c requires an argument.\n");
							exit_status = EXIT_FAILURE;
							return true;
						}
						config_dir = arg[++i];
						break;
					case 'e':
						experimental = true;
						break;
					case 'v':
						verbosity++;
						break;
					case 'h':
						usage(arg[0]);
						exit_status = EXIT_SUCCESS;
						return true;
					default:
						printf("Error: Unknown option -%c\n", arg[i][j]);
						exit_status = EXIT_FAILURE;
						return true;
				}
		}
		i++;
	}

	if (i > 1) {
		for (int j = 1; j < i; j++) {
			g_free(arg[j]);
			arg[j] = 0;
		}
		for (int j = 0; arg[j+i]; j++) {
			arg[j+1] = arg[j+i];
			arg[j+i] = 0;
		}
	}

	if (!register_application()) {
		printf("Failed to register the application\n");
		exit_status = EXIT_FAILURE;
		return true;
	}
	activate();
	return false;
}

void App::run_by_name(const char *str, const Glib::RefPtr<Gio::ApplicationCommandLine> &cmd_line) {
	for (ActionDB::const_iterator i = actions.begin(); i != actions.end(); i++) {
		if (i->second.name == std::string(str)) {
			if (i->second.action)
				xstate->queue(sigc::bind(sigc::mem_fun(xstate, &XState::run_action), i->second.action));
			return;
		}
	}
	char *msg;
	asprintf(&msg, _("Warning: No action \"%s\" defined\n"), str);
	cmd_line->print(msg);
	free(msg);
}

int App::on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &command_line) {
	int argc;
	char **arg = command_line->get_arguments(argc);
	for (int i = 1; arg[i]; i++)
		if (!strcmp(arg[i], "send")) {
			if (!arg[++i])
				printf("Warning: Send requires an argument\n");
			else
				run_by_name(arg[i], command_line);
		} else if (!strcmp(arg[i], "show")) {
			win->show();
		} else if (!strcmp(arg[i], "hide")) {
			win->hide();
		} else if (!strcmp(arg[i], "disable")) {
			disabled.set(true);
		} else if (!strcmp(arg[i], "enable")) {
			disabled.set(false);
		} else if (!strcmp(arg[i], "about")) {
			win->show_about();
		} else if (!strcmp(arg[i], "quit")) {
			quit();
		} else {
			char *msg;
			asprintf(&msg, "Warning: Unknown command \"%s\".\n", arg[i]);
			command_line->print(msg);
			free(msg);
		}
	if (!arg[1] && remote)
		win->show_hide();
	remote = true;
	return true;
}

static void enabled_activated(GSimpleAction *simple, GVariant *parameter, gpointer user_data) {
	disabled.set(!disabled.get());
}

void App::on_startup() {
	Gtk::Application::on_startup();

	Glib::RefPtr<Gio::SimpleAction> action;
        action = Gio::SimpleAction::create("about");
	action->signal_activate().connect(sigc::mem_fun(*this, &App::on_about));
	add_action(action);

        action = Gio::SimpleAction::create("quit");
	action->signal_activate().connect(sigc::mem_fun(*this, &App::on_quit));
	add_action(action);

	enabled = g_simple_action_new_stateful("enabled", 0, g_variant_new_boolean(false));
	g_signal_connect(G_OBJECT(enabled), "activate", G_CALLBACK(enabled_activated), this);
	g_action_map_add_action(G_ACTION_MAP(gobj()), G_ACTION(enabled));

	Glib::RefPtr<Gio::Menu> menu = Gio::Menu::create();
	menu->append(_("Enabled"), "app.enabled");
	menu->append(_("About"), "app.about");
	menu->append(_("Quit"), "app.quit");
	set_app_menu(menu);

	disabled.connect(this);
	notify();
}

void App::on_activate() {
	if (win)
		return;

	bindtextdomain("easystroke", is_dir("po") ? "po" : LOCALEDIR);
	bind_textdomain_codeset("easystroke", "UTF-8");
	textdomain("easystroke");

	create_config_dir();
	unsetenv("DESKTOP_AUTOSTART_ID");
	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		printf(_("Couldn't open display.\n"));
		exit(EXIT_FAILURE);
	}

	ROOT = DefaultRootWindow(dpy);

	prefs.init();
	action_watcher = new ActionDBWatcher;
	action_watcher->init();

	xstate = new XState;
	new Grabber;
	// Force enter events to be generated
	XGrabPointer(dpy, ROOT, False, 0, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
	XUngrabPointer(dpy, CurrentTime);

	trace.reset(init_trace());
	Glib::RefPtr<Gdk::Screen> screen = Gdk::Display::get_default()->get_default_screen();
	g_signal_connect(screen->gobj(), "composited-changed", &schedule_reload_trace, NULL);
	screen->signal_size_changed().connect(sigc::ptr_fun(&schedule_reload_trace));
	Notifier *trace_notify = new Notifier(sigc::ptr_fun(&schedule_reload_trace));
	prefs.trace.connect(trace_notify);
	prefs.color.connect(trace_notify);

	XTestGrabControl(dpy, True);

	Glib::RefPtr<Glib::IOSource> io = Glib::IOSource::create(ConnectionNumber(dpy), Glib::IO_IN);
	io->connect(sigc::hide(sigc::mem_fun(*xstate, &XState::handle)));
	io->attach();
	try {
		widgets = Gtk::Builder::create_from_string(gui_buffer);
	} catch (Gtk::BuilderError &e) {
		printf("Error building GUI: %s\n", e.what().c_str());
		exit(EXIT_FAILURE);
	}
	win = new Win;
	add_window(win->get_window());
	if (!actions.get_root()->size_rec())
		win->get_window().show();
	hold();
}

void App::usage(const char *me) {
	printf("The full easystroke documentation is available at the following address:\n");
	printf("\n");
	printf("http://easystroke.wiki.sourceforge.net/Documentation#content\n");
	printf("\n");
	printf("Usage: %s [OPTION]... [COMMAND]...\n", me);
	printf("\n");
	printf("Commands:\n");
	printf("  send <action_name>     Execute action <action_name>\n");
	printf("  show                   Show configuration window\n");
	printf("  hide                   Hide configuration window\n");
	printf("  disable                Disable easystroke\n");
	printf("  enable                 Enable easystroke\n");
	printf("  about                  Show about dialog\n");
	printf("  quit                   Quit easystroke\n");
	printf("\n");
	printf("Options:\n");
	printf("  -c, --config-dir <dir> Directory for config files\n");
	printf("  -e  --experimental     Start in experimental mode\n");
	printf("  -v, --verbose          Increase verbosity level\n");
	printf("  -h, --help             Display this help and exit\n");
	printf("      --version          Output version information and exit\n");
}

extern const char *version_string;
void App::version() {
	printf("easystroke %s\n", version_string);
	printf("\n");
	printf("Written by Thomas Jaeger <ThJaeger@gmail.com>.\n");
	exit(EXIT_SUCCESS);
}

void App::create_config_dir() {
	if (config_dir == "") {
		config_dir = getenv("HOME");
		config_dir += "/.easystroke";
	}
	struct stat st;
	char *name = realpath(config_dir.c_str(), NULL);

	// check if the directory does not exist
	if (lstat(name, &st) == -1) {
		if (mkdir(config_dir.c_str(), 0777) == -1) {
			printf(_("Error: Couldn't create configuration directory \"%s\"\n"), config_dir.c_str());
			exit(EXIT_FAILURE);
		}
	} else {
		if (!S_ISDIR(st.st_mode)) {
			printf(_("Error: \"%s\" is not a directory\n"), config_dir.c_str());
			exit(EXIT_FAILURE);
		}


	}
	free (name);
	config_dir += "/";
}

App::~App() {
	if (win) {
		delete win;
		trace->end();
		trace.reset();
		delete grabber;
		XCloseDisplay(dpy);
		prefs.execute_now();
		action_watcher->execute_now();
	}
}

int main(int argc, char **argv) {
	if (0) {
		RStroke trefoil = Stroke::trefoil();
		trefoil->draw_svg("easystroke.svg");
		exit(EXIT_SUCCESS);
	}
	App app(argc, argv, "org.easystroke.easystroke", Gio::APPLICATION_HANDLES_COMMAND_LINE);
	return app.run(argc, argv);
}

void Button::run() {
	xstate->fake_click(button);
}

void SendKey::run() {
	if (!key)
		return;
	guint code = XKeysymToKeycode(dpy, key);
	XTestFakeKeyEvent(dpy, code, true, 0);
	XTestFakeKeyEvent(dpy, code, false, 0);
}

void fake_unicode(gunichar c) {
	static const KeySym numcode[10] = { XK_0, XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8, XK_9 };
	static const KeySym hexcode[6] = { XK_a, XK_b, XK_c, XK_d, XK_e, XK_f };

	if (verbosity >= 3) {
		char buf[7];
		buf[g_unichar_to_utf8(c, buf)] = '\0';
		printf("using unicode input for character %s\n", buf);
	}
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Control_L), true, 0);
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Shift_L), true, 0);
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_u), true, 0);
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_u), false, 0);
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Shift_L), false, 0);
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_Control_L), false, 0);
	char buf[16];
	snprintf(buf, sizeof(buf), "%x", c);
	for (int i = 0; buf[i]; i++)
		if (buf[i] >= '0' && buf[i] <= '9') {
			XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, numcode[buf[i]-'0']), true, 0);
			XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, numcode[buf[i]-'0']), false, 0);
		} else if (buf[i] >= 'a' && buf[i] <= 'f') {
			XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, hexcode[buf[i]-'a']), true, 0);
			XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, hexcode[buf[i]-'a']), false, 0);
		}
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_space), true, 0);
	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, XK_space), false, 0);
}

bool fake_char(gunichar c) {
	char buf[16];
	snprintf(buf, sizeof(buf), "U%04X", c);
	KeySym keysym = XStringToKeysym(buf);
	if (keysym == NoSymbol)
		return false;
	KeyCode keycode = XKeysymToKeycode(dpy, keysym);
	if (!keycode)
		return false;
	KeyCode modifier = 0;
	int n;
	KeySym *mapping = XGetKeyboardMapping(dpy, keycode, 1, &n);
	if (mapping[0] != keysym) {
		int i;
		for (i = 1; i < n; i++)
			if (mapping[i] == keysym)
				break;
		if (i == n)
			return false;
		XModifierKeymap *keymap = XGetModifierMapping(dpy);
		modifier = keymap->modifiermap[i];
		XFreeModifiermap(keymap);
	}
	XFree(mapping);
	if (modifier)
		XTestFakeKeyEvent(dpy, modifier, true, 0);
	XTestFakeKeyEvent(dpy, keycode, true, 0);
	XTestFakeKeyEvent(dpy, keycode, false, 0);
	if (modifier)
		XTestFakeKeyEvent(dpy, modifier, false, 0);
	return true;
}

void SendText::run() {
	for (Glib::ustring::iterator i = text.begin(); i != text.end(); i++)
		if (!fake_char(*i))
			fake_unicode(*i);
}

static struct {
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
static int n_modkeys = 10;

class Modifiers : Timeout {
	static std::set<Modifiers *> all;
	static void update_mods() {
		static guint mod_state = 0;
		guint new_state = 0;
		for (std::set<Modifiers *>::iterator i = all.begin(); i != all.end(); i++)
			new_state |= (*i)->mods;
		for (int i = 0; i < n_modkeys; i++) {
			guint mask = modkeys[i].mask;
			if ((mod_state & mask) ^ (new_state & mask))
				XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, modkeys[i].sym), new_state & mask, 0);
		}
		mod_state = new_state;
	}

	guint mods;
	Glib::ustring str;
	OSD *osd;
public:
	Modifiers(guint mods_, Glib::ustring str_) : mods(mods_), str(str_), osd(NULL) {
		if (prefs.show_osd.get())
			set_timeout(150);
		all.insert(this);
		update_mods();
	}
	bool operator==(const Modifiers &m) {
		return mods == m.mods && str == m.str;
	}
	virtual void timeout() {
		osd = new OSD(str);
	}
	~Modifiers() {
		all.erase(this);
		update_mods();
		delete osd;
	}
};
std::set<Modifiers *> Modifiers::all;

RModifiers ModAction::prepare() {
	return RModifiers(new Modifiers(mods, get_label()));
}

RModifiers SendKey::prepare() {
	if (!mods)
		return RModifiers();
	return RModifiers(new Modifiers(mods, ModAction::get_label()));
}

bool mods_equal(RModifiers m1, RModifiers m2) {
	return m1 && m2 && *m1 == *m2;
}

void Misc::run() {
	switch (type) {
		case SHOWHIDE:
			win->show_hide();
			return;
		case UNMINIMIZE:
			grabber->unminimize();
			return;
		case DISABLE:
			disabled.set(!disabled.get());
			return;
		default:
			return;
	}
}

bool is_file(std::string filename) {
	struct stat st;
	return lstat(filename.c_str(), &st) != -1 && S_ISREG(st.st_mode);
}

bool is_dir(std::string dirname) {
	struct stat st;
	return lstat(dirname.c_str(), &st) != -1 && S_ISDIR(st.st_mode);
}
