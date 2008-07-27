#ifndef __PREFDB_H__
#define __PREFDB_H__
#include <string>
#include <set>
#include <boost/serialization/access.hpp>
#include <boost/serialization/version.hpp>

#include "locking.h"
#include "var.h"

enum TraceType { TraceStandard, TraceShape, TraceNone };
const int trace_n = 3;

struct ButtonInfo {
	guint button;
	guint state;
	void press();
	Glib::ustring get_button_text();
private:
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version) {
		ar & button;
		ar & state;
		if (version == 1) {
			int special;
			ar & special;
			return;
		}
	}
};
BOOST_CLASS_VERSION(ButtonInfo, 2)

extern const double default_p;
extern const int default_radius;
extern const int default_pressure_threshold;

class PrefDB {
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version) {
		Setter s;
		ar & s.ref(exceptions);
		ar & s.ref(p);
		ar & s.ref(button);
		if (version <= 1) {
			bool help;
			ar & help;
		}
		ar & s.ref(trace);
		if (version <= 2) {
			int delay;
			ar & delay;
		}
		if (version == 1) {
			ButtonInfo foo;
			ar & foo;
			ar & foo;
			return;
		}
		if (version <= 1) return;
		ar & s.ref(advanced_ignore);
		ar & s.ref(radius);
		if (version <= 3) return;
		ar & s.ref(ignore_grab);
		ar & s.ref(timing_workaround);
		ar & s.ref(show_clicks);
		ar & s.ref(pressure_abort);
		ar & s.ref(pressure_threshold);
		ar & s.ref(proximity);
	}
	std::string filename;
public:
	PrefDB();

	VarI<std::set<std::string> > exceptions;
	VarI<double> p;
	VarI<ButtonInfo> button;
	VarI<TraceType> trace;
	VarI<bool> advanced_ignore;
	VarI<int> radius;
	VarI<bool> ignore_grab;
	VarI<bool> timing_workaround;
	VarI<bool> show_clicks;
	VarI<bool> pressure_abort;
	VarI<int> pressure_threshold;
	VarI<bool> proximity;

	void read();
	bool write() const;
};

BOOST_CLASS_VERSION(PrefDB, 4)

PrefDB& prefs();
#endif
