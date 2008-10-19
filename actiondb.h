/*
 * Copyright (c) 2008, Thomas Jaeger <ThJaeger@gmail.com>
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
class Misc;
class Unique {
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
};

typedef boost::shared_ptr<Action> RAction;
typedef boost::shared_ptr<Command> RCommand;
typedef boost::shared_ptr<SendKey> RSendKey;
typedef boost::shared_ptr<Scroll> RScroll;
typedef boost::shared_ptr<Ignore> RIgnore;
typedef boost::shared_ptr<Button> RButton;
typedef boost::shared_ptr<Misc> RMisc;


class Action {
	friend class boost::serialization::access;
	friend std::ostream& operator<<(std::ostream& output, const Action& c);
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
public:
	virtual bool run() = 0;
	virtual const Glib::ustring get_label() const = 0;
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
	virtual const Glib::ustring get_label() const { return cmd; }
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
	BOOST_SERIALIZATION_SPLIT_MEMBER()
	template<class Archive> void load(Archive & ar, const unsigned int version);
	template<class Archive> void save(Archive & ar, const unsigned int version) const;
	SendKey(guint key_, Gdk::ModifierType mods, guint code_) :
		ModAction(mods), key(key_), code(code_) { compute_code(); }

	void compute_code();
public:
	SendKey() {}
	static RSendKey create(guint key, Gdk::ModifierType mods, guint code) {
		return RSendKey(new SendKey(key, mods, code));
	}

	virtual bool run();
	virtual const Glib::ustring get_label() const;
};
BOOST_CLASS_VERSION(SendKey, 1)

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

class Misc : public Action {
	friend class boost::serialization::access;
public:
	enum Type { NONE, UNMINIMIZE, SHOWHIDE, DISABLE };
private:
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
	Misc(Type t) : type(t) {}
	Type type;
public:
	static const char *types[5];
	Misc() {}
	virtual const Glib::ustring get_label() const { return types[type]; }
	static RMisc create(Type t) { return RMisc(new Misc(t)); }
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
	std::string name;
};
typedef boost::shared_ptr<StrokeInfo> RStrokeInfo;
BOOST_CLASS_VERSION(StrokeInfo, 1)

struct Ranking {
	RStroke stroke, best_stroke;
	RAction action;
	double score;
	Unique *id;
	std::string name;
	std::multimap<double, std::pair<std::string, RStroke> > r;
	int x, y;
	bool show();
};

class ActionListDiff {
	friend class boost::serialization::access;
	friend class ActionDB;
	template<class Archive> void serialize(Archive & ar, const unsigned int version);
	ActionListDiff *parent;
	std::set<Unique *> deleted;
	std::map<Unique *, StrokeInfo> added;
	std::list<ActionListDiff> children;
public:
	bool app;
	std::string name;

	ActionListDiff() : parent(0), app(false) {}

	typedef std::list<ActionListDiff>::iterator iterator;
	iterator begin() { return children.begin(); }
	iterator end() { return children.end(); }

	RStrokeInfo get_info(Unique *id, bool *deleted = 0, bool *stroke = 0, bool *name = 0, bool *action = 0) const;

	Unique *add(StrokeInfo &si) {
		Unique *id = new Unique;
		added.insert(std::pair<Unique *, StrokeInfo>(id, si));
		return id;
	}
	void set_action(Unique *id, RAction action) { added[id].action = action; }
	void set_strokes(Unique *id, StrokeSet strokes) { added[id].strokes = strokes; }
	void set_name(Unique *id, std::string name) { added[id].name = name; }
	bool contains(Unique *id) {
		if (deleted.count(id))
			return false;
		if (added.count(id))
			return true;
		return parent && parent->contains(id);
	}
	bool remove(Unique *id) {
		bool really = !(parent && parent->contains(id));
		if (really)
			added.erase(id);
		else
			deleted.insert(id);
		for (std::list<ActionListDiff>::iterator i = children.begin(); i != children.end(); i++)
			i->remove(id);
		return really;
	}
	void reset(Unique *id) {
		added.erase(id);
		deleted.erase(id);
	}
	void add_apps(std::map<std::string, ActionListDiff *> &apps) {
		if (app)
			apps[name] = this;
		for (std::list<ActionListDiff>::iterator i = children.begin(); i != children.end(); i++)
			i->add_apps(apps);
	}
	ActionListDiff *add_child(std::string name, bool app) {
		children.push_back(ActionListDiff());
		ActionListDiff *child = &(children.back());
		child->name = name;
		child->app = app;
		child->parent = this;
		return child;
	}

	boost::shared_ptr<std::map<Unique *, StrokeSet> > get_strokes() const;
	boost::shared_ptr<std::set<Unique *> > get_ids(bool include_deleted) const;
	void all_strokes(std::list<RStroke> &strokes) const;
	Ranking *handle(RStroke, int) const;
};

extern Unique stroke_not_found, stroke_is_click, stroke_is_timeout;

class ActionDB {
	friend class boost::serialization::access;
	friend class ActionDBWatcher;
	template<class Archive> void load(Archive & ar, const unsigned int version);
	template<class Archive> void save(Archive & ar, const unsigned int version) const;
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	ActionListDiff root;
public:
	typedef std::map<Unique *, StrokeInfo>::const_iterator const_iterator;
	const const_iterator begin() const { return root.added.begin(); }
	const const_iterator end() const { return root.added.end(); }

	std::map<std::string, ActionListDiff *> apps;
	ActionListDiff *get_root() { return &root; }

	const ActionListDiff *get_action_list(std::string wm_class) const {
		std::map<std::string, ActionListDiff *>::const_iterator i = apps.find(wm_class);
		return i == apps.end() ? &root : i->second;
	}
};
BOOST_CLASS_VERSION(ActionDB, 2)

class ActionDBWatcher : public TimeoutWatcher {
	bool good_state;
public:
	ActionDBWatcher() : TimeoutWatcher(5000), good_state(true) {}
	void init();
	virtual void timeout();
};

extern ActionDB actions;
void update_actions();
#endif
