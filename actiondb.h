#ifndef __STROKEDB_H__
#define __STROKEDB_H__
#include <string>
#include <map>
#include <set>
#include <boost/serialization/access.hpp>
#include <iostream>

#include "stroke.h"
#include "locking.h"

class Action;
class Command;
class SendKey;
class Scroll;
class Ignore;

typedef boost::shared_ptr<Action> RAction;
typedef boost::shared_ptr<Command> RCommand;
typedef boost::shared_ptr<SendKey> RSendKey;
typedef boost::shared_ptr<Scroll> RScroll;
typedef boost::shared_ptr<Ignore> RIgnore;

class Action {
	friend class boost::serialization::access;
	friend std::ostream& operator<<(std::ostream& output, const Action& c);
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
public:
	virtual bool run() = 0;
        virtual std::ostream& show(std::ostream& output) const { return output; }
	virtual ~Action() {};
};

class Command : public Action {
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
	Command(const std::string &c) : cmd(c) {}
public:
	std::string cmd;
	Command() {}
	static RCommand create(const std::string &c) { return RCommand(new Command(c)); }
	virtual bool run();
};

class ModAction : public Action {
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
protected:
	ModAction() {}
	Gdk::ModifierType mods;
	ModAction(Gdk::ModifierType mods_) : mods(mods_) {}
	void press();
	void release();
public:
	virtual const Glib::ustring get_label() const;
};

class SendKey : public ModAction {
	friend class boost::serialization::access;
	guint key;
	guint code;
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
	SendKey(guint key_, Gdk::ModifierType mods, guint code_, bool xtest_ = false) :
		ModAction(mods), key(key_), code(code_), xtest(xtest_) {}
public:
	SendKey() {}
	static RSendKey create(guint key, Gdk::ModifierType mods, guint code, bool xtest = false) {
		return RSendKey(new SendKey(key, mods, code, xtest));
	}

	virtual bool run();
	virtual const Glib::ustring get_label() const;

	bool xtest;
};

class Scroll : public ModAction {
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
	void worker();
	Scroll(Gdk::ModifierType mods) : ModAction(mods) {}
public:
	Scroll() {}
	static RScroll create(Gdk::ModifierType mods) { return RScroll(new Scroll(mods)); }
	virtual bool run();
};

class Ignore : public ModAction {
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
	Ignore(Gdk::ModifierType mods) : ModAction(mods) {}
public:
	Ignore() {}
	static RIgnore create(Gdk::ModifierType mods) { return RIgnore(new Ignore(mods)); }
	virtual bool run();
};

class StrokeSet : public std::set<RStroke> {
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
};

class StrokeInfo {
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
public:
	StrokeInfo(RStroke s, RAction a) : action(a) { strokes.insert(s); }
	StrokeInfo() {};
	StrokeSet strokes;
	RAction action;
};


struct Ranking {
	RStroke stroke;
	RAction action;
	double score;
	std::string name;
	std::multimap<double, std::pair<std::string, RStroke> > r;
};

class StrokeIterator {
	const std::map<std::string, StrokeInfo> &names;
	std::map<std::string, StrokeInfo>::const_iterator i;
	const StrokeSet *strokes;
	StrokeSet::const_iterator j;
	void init_j() {
		strokes = &(i->second.strokes);
		j = strokes->begin();
	}
	void next() {
		while (1) {
			while (j == strokes->end()) {
				i++;
				if (i == names.end())
					return;
				init_j();
			}
			if (*j)
				return;
			j++;
		}
	}
public:
	// This is why C++ sucks balls. It's really easy to shoot yourself in
	// the foot even if you know what you're doing.  In this case I forgot
	// the `&'.  Took me 2 hours to figure out what was going wrong.
	StrokeIterator(const std::map<std::string, StrokeInfo> &names_) : names(names_) {
		i = names.begin();
		if (i == names.end())
			return;
		init_j();
		next();
	}
	operator bool() {
		return i != names.end() && j != strokes->end();
	}
	void operator++(int) {
		j++;
		next();
	}
	const std::string& name() {
		return i->first;
	}
	// Guaranteed to be dereferencable
	RStroke stroke() {
		return *j;
	}
	RAction action() {
		return i->second.action;
	}

};

class ActionDB {
	friend class boost::serialization::access;
	std::map<std::string, StrokeInfo> strokes;
	template<class Archive> void serialize(Archive & ar, const unsigned int version) {
		ar & strokes;
	}
	std::string filename;
public:
	ActionDB();
	typedef std::map<std::string, StrokeInfo>::const_iterator const_iterator;
	const const_iterator begin() const { return strokes.begin(); }
	const const_iterator end() const { return strokes.end(); }
	StrokeIterator strokes_begin() { return StrokeIterator(strokes); }

	StrokeInfo &operator[](const std::string &name) { return strokes[name]; }

	void read();
	bool write() const;

	bool has(const std::string& name) const { return strokes.find(name) != strokes.end(); }
	bool remove(const std::string& name);
	bool rename(const std::string& name, const std::string& name2);
	int nested_size() const;
	bool addCmd(RStroke, const std::string& name, const std::string& cmd);
	bool changeCmd(const std::string& name, const std::string& cmd);
	Ranking handle(RStroke);
};

class LActionDB : public Lock<ActionDB> {
public:
	void read() { Ref<ActionDB> ref(*this); ref->read(); }
	Ranking handle(RStroke s) { Ref<ActionDB> ref(*this); return ref->handle(s); }
	bool remove(const std::string& name) { Ref<ActionDB> ref(*this); return ref->remove(name); }
	bool rename(const std::string& name, const std::string& name2) { Ref<ActionDB> ref(*this); return ref->rename(name, name2); }
	bool addCmd(RStroke s, const std::string& name, const std::string& cmd) { Ref<ActionDB> ref(*this); return ref->addCmd(s, name, cmd); }
	bool changeCmd(const std::string& name, const std::string& cmd) { Ref<ActionDB> ref(*this); return ref->changeCmd(name, cmd); }
};

LActionDB& actions();
#endif
