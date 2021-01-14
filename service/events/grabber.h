#pragma once

#include <gtkmm.h>
#include <map>
#include <string>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>

#include "prefdb.h"

#include "device.h"

#define MAX_BUTTONS 256

class Grabber {
	friend class Handler;
	friend class StrokeHandler;
public:
	enum State { NONE, BUTTON, SELECT, RAW };
	static const char *state_name[4];

	typedef std::map<XID, std::shared_ptr<Events::XiDevice> > DeviceMap;
	int opcode, event, error;
	Events::XiDevice *get_xi_dev(int id);
private:
	bool init_xi();

	DeviceMap xi_devs;
	State current, grabbed;
	bool xi_grabbed;
	Events::GrabState xi_devs_grabbed;
	int suspended;
	bool active;
	Cursor cursor_select;
	ButtonInfo grabbed_button;
	std::vector<ButtonInfo> buttons;

	void set();
	void grab_xi(bool);
	void grab_xi_devs(Events::GrabState);

	void update_excluded();

	void grab(State s) { current = s; set(); }
public:
	Grabber();
	~Grabber();

	void new_device(XIDeviceInfo *);

	bool is_instant(guint b);
	bool is_click_hold(guint b);
	bool hierarchy_changed(XIHierarchyEvent *);

	int get_default_button() { return grabbed_button.button; }
	guint get_default_mods(guint button);

    void suspend() { suspended++; set(); }
    void resume() { if (suspended) suspended--; set(); }
    void update();
};

class GrabFailedException : public std::exception {
	char *msg;
public:
	GrabFailedException(int code) { if (asprintf(&msg, "Grab Failed: %d", code) == -1) msg = nullptr; }
	virtual const char* what() const throw() { return msg ? msg : "Grab Failed"; }
	~GrabFailedException() throw() { free(msg); }
};

namespace grabbers {
    Window get_app_window(Window w);
}
