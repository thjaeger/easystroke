#include "prefdb.h"
#include <X11/X.h>

template<class Key, class T>
std::shared_ptr<std::map<Key,T>>
make_shared_map(std::initializer_list<typename std::map<Key,T>::value_type> il)
{
    return std::make_shared<std::map<Key,T>>(il);
}

PrefDB::PrefDB() :
    exceptions(make_shared_map<std::string, RButtonInfo>({})),
	button(Button2),
	proximity(false),
	init_timeout(250),
	final_timeout(250),
	timeout_profile(TimeoutDefault),
	timeout_gestures(false),
	excluded_devices(std::make_shared<std::set<std::string>>()),
	color(Gdk::Color("#980101")),
	trace_width(3),
	extra_buttons(std::make_shared<std::vector<ButtonInfo>>()),
	scroll_invert(true),
	scroll_speed(2.0),
    move_back(false),
	device_timeout(make_shared_map<std::string, TimeoutType>({})),
    whitelist(false)
{}

bool ButtonInfo::overlap(const ButtonInfo &bi) const {
	if (button != bi.button)
		return false;
	if (state == AnyModifier || bi.state == AnyModifier)
		return true;
	return !((state ^ bi.state) & ~LockMask & ~Mod2Mask);
}

PrefDB prefs{};
