#ifndef __PREFDB_H__
#define __PREFDB_H__
#include <string>
#include <set>
#include <boost/serialization/access.hpp>
#include <boost/serialization/version.hpp>

#include "locking.h"

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

extern const double pref_p_default;
extern const int pref_delay_default;

class PrefDB {
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version) {
		{ Ref<std::set<std::string> > ref(exceptions); ar & *ref; }
		{ Ref<double> ref(p); ar & *ref; }
		{ Ref<ButtonInfo> ref(button); ar & *ref; }
		if (version <= 1) { 
			bool help; 
			ar & help; 
		}
		{ Ref<TraceType> ref(trace); ar & *ref; }
		{ Ref<int> ref(delay); ar & *ref; }
		if (version == 1) {
			ButtonInfo foo; 
			ar & foo;
			ar & foo;
			return;
		}
		if (version <= 1) return;
		{ Ref<bool> ref(cds_stroke); ar & *ref; }
	}
	std::string filename;
public:
	PrefDB();

	Lock<std::set<std::string> > exceptions;
	Lock<double> p;
	Lock<ButtonInfo> button;
	Lock<TraceType> trace;
	Lock<int> delay;
	Lock<bool> cds_stroke;

	void read();
	bool write() const;
};

BOOST_CLASS_VERSION(PrefDB, 2)

typedef Ref<std::set<std::string> > RPrefEx;

PrefDB& prefs();
#endif
