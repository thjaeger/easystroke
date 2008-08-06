/*
 * Copyright (c) 2008, Thomas Jaeger <ThJaeger@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __STROKEDB_H__
#define __STROKEDB_H__
#include <string>
#include <map>
#include <set>
#include <boost/serialization/access.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/split_member.hpp>
#include <iostream>

#include "stroke.h"
#include "prefdb.h"

class Action;
class Command;
class SendKey;
class Scroll;
class Ignore;
class Button;

typedef boost::shared_ptr<Action> RAction;
typedef boost::shared_ptr<Command> RCommand;
typedef boost::shared_ptr<SendKey> RSendKey;
typedef boost::shared_ptr<Scroll> RScroll;
typedef boost::shared_ptr<Ignore> RIgnore;
typedef boost::shared_ptr<Button> RButton;

class Action {
	friend class boost::serialization::access;
	friend std::ostream& operator<<(std::ostream& output, const Action& c);
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
public:
	virtual bool run() = 0;
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

class Button : public ModAction {
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
	Button(Gdk::ModifierType mods, guint button_) : ModAction(mods), button(button_) {}
	guint button;
public:
	Button() {}
	ButtonInfo get_button_info() const;
	static RButton create(Gdk::ModifierType mods, guint button_) { return RButton(new Button(mods, button_)); }
	virtual bool run();
	virtual const Glib::ustring get_label() const;
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
	std::string name;
};
BOOST_CLASS_VERSION(StrokeInfo, 1)


struct Ranking {
	RStroke stroke, best_stroke;
	RAction action;
	double score;
	int id;
	std::string name;
	std::multimap<double, std::pair<std::string, RStroke> > r;
	int x, y;
};

class StrokeIterator {
	const std::map<int, StrokeInfo> &ids;
	std::map<int, StrokeInfo>::const_iterator i;
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
				if (i == ids.end())
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
	StrokeIterator(const std::map<int, StrokeInfo> &ids_) : ids(ids_) {
		i = ids.begin();
		if (i == ids.end())
			return;
		init_j();
		next();
	}
	operator bool() {
		return i != ids.end() && j != strokes->end();
	}
	void operator++(int) {
		j++;
		next();
	}
	const int& id() {
		return i->first;
	}
	const std::string name() {
		return i->second.name;
	}
	// Guaranteed to be dereferencable
	RStroke stroke() {
		return *j;
	}
	RAction action() {
		return i->second.action;
	}

};

class ActionDB : public TimeoutWatcher {
	friend class boost::serialization::access;
	std::map<int, StrokeInfo> strokes;
	bool good_state;
	template<class Archive> void load(Archive & ar, const unsigned int version);
	template<class Archive> void save(Archive & ar, const unsigned int version) const {
		ar & strokes;
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	int current_id;
	int add(StrokeInfo &);
public:
	ActionDB();
	typedef std::map<int, StrokeInfo>::const_iterator const_iterator;
	const const_iterator begin() const { return strokes.begin(); }
	const const_iterator end() const { return strokes.end(); }
	StrokeIterator strokes_begin() const { return StrokeIterator(strokes); }

	const StrokeInfo &lookup(int id) const { return strokes.find(id)->second; }
	StrokeInfo &operator[](int id) { return strokes[id]; }

	void init();
	void timeout();

	int size() const { return strokes.size(); }
	bool remove(int id);
	int nested_size() const;
	int addCmd(RStroke, const std::string& name, const std::string& cmd);
	Ranking *handle(RStroke) const;
};
BOOST_CLASS_VERSION(ActionDB, 1)

extern Var<ActionDB> actions;
#endif
