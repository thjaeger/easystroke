#pragma once

#include <gtkmm.h>
#include <map>
#include <string>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>


#include "device.h"
#include "deviceobserver.h"
#include "prefdb.h"
#include "pointergrabber.h"

#define MAX_BUTTONS 256

class Grabber {
	friend class Handler;
	friend class StrokeHandler;
private:
    std::unique_ptr<Events::PointerGrabber> pointer;

public:
    // TODO: Should be private
    std::unique_ptr<Events::DeviceObserver> devices;

	enum State { NONE, BUTTON, SELECT, RAW };

private:

	State current, grabbed;
	bool xi_grabbed;
	Events::GrabState xi_devs_grabbed;
	int suspended;
	bool active;
	ButtonInfo grabbed_button;
	std::vector<ButtonInfo> buttons;

	void set();
	void grab_xi(bool);
	void grab_xi_devs(Events::GrabState);

	void grab(State s) { current = s; set(); }
public:
	Grabber();


	bool is_instant(guint b);
	bool is_click_hold(guint b);

	int get_default_button() { return grabbed_button.button; }
	guint get_default_mods(guint button);

    void suspend() { suspended++; set(); }
    void resume() { if (suspended) suspended--; set(); }
    void update();
};
