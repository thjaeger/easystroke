#pragma once

#include <gtkmm.h>
#include <map>
#include <string>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>

#include "buttongrabber.h"
#include "device.h"
#include "devicegrabber.h"
#include "deviceobserver.h"
#include "prefdb.h"
#include "pointergrabber.h"

#define MAX_BUTTONS 256

class Grabber {
	friend class Handler;
	friend class StrokeHandler;
private:
    std::unique_ptr<Events::PointerGrabber> pointer;
    explicit Grabber(const std::shared_ptr<Events::DeviceObserver>& deviceObserver);

public:
    // TODO: Should be private
    std::shared_ptr<Events::DeviceObserver> devices;
    std::unique_ptr<Events::DeviceGrabber> deviceGrabber;
    std::unique_ptr<Events::ButtonGrabber> buttonGrabber;

	enum State { NONE, BUTTON, SELECT, RAW };

private:

	State current, grabbed;
	int suspended;
	bool active;

	void set();

	void grab(State s) { current = s; set(); }
public:
	Grabber();

    void suspend() { suspended++; set(); }
    void resume() { if (suspended) suspended--; set(); }
    void update();
};
