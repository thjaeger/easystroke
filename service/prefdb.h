#pragma once
#include <string>
#include <set>
#include <map>
#include <memory>
#include <gdkmm/color.h>

#include "var.h"

enum TimeoutType { TimeoutOff, TimeoutDefault, TimeoutMedium, TimeoutAggressive, TimeoutFlick, TimeoutCustom, TimeoutConservative };

class ButtonInfo {
public:
	guint button;
	guint state;
	bool instant;
	bool click_hold;
	bool operator<(const ButtonInfo &bi) const { return button < bi.button; }
	bool operator==(const ButtonInfo &bi) const {
		return button == bi.button && state == bi.state && !instant == !bi.instant && !click_hold == !bi.click_hold;
	}
	void press();
	Glib::ustring get_button_text() const;
	bool overlap(const ButtonInfo &bi) const;
	ButtonInfo(guint button_) : button(button_), state(0), instant(false), click_hold(false) {}
	ButtonInfo() : button(0), state(0), instant(false), click_hold(false) {}
};

typedef std::shared_ptr<ButtonInfo> RButtonInfo;

struct RGBA {
	Gdk::Color color;
	guint16 alpha;
	RGBA() : alpha(65535) {}
	RGBA(Gdk::Color c) : color(c), alpha(65535) {}

	bool operator==(const RGBA rgba) {
		return color == rgba.color && alpha == rgba.alpha;
	}
};

class PrefDB {

public:
	PrefDB();

	std::shared_ptr<std::map<std::string, RButtonInfo>> exceptions;
	ButtonInfo button;
	bool proximity;
	int init_timeout;
	int final_timeout;
	TimeoutType timeout_profile;
	bool timeout_gestures;
	std::shared_ptr<std::set<std::string>> excluded_devices;
	RGBA color;
	int trace_width;
	std::shared_ptr<std::vector<ButtonInfo>> extra_buttons;
	bool scroll_invert;
	double scroll_speed;
	bool move_back;
	std::shared_ptr<std::map<std::string, TimeoutType>> device_timeout;
	bool whitelist;
};

extern PrefDB prefs;
