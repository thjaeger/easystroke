#pragma once
#include <string>
#include <map>
#include <set>
#include <iostream>
#include <utility>

#include "gesture.h"
#include "prefdb.h"

class Action;
class Command;
class SendKey;
class SendText;
class Scroll;
class Ignore;
class Button;
class Misc;
class Ranking;

typedef std::shared_ptr<Action> RAction;
typedef std::shared_ptr<SendKey> RSendKey;
typedef std::shared_ptr<SendText> RSendText;
typedef std::shared_ptr<Scroll> RScroll;
typedef std::shared_ptr<Ignore> RIgnore;
typedef std::shared_ptr<Button> RButton;
typedef std::shared_ptr<Misc> RMisc;
typedef std::shared_ptr<Ranking> RRanking;

class Unique;

class Modifiers;
typedef std::shared_ptr<Modifiers> RModifiers;

bool mods_equal(RModifiers m1, RModifiers m2);

class Action {
public:
	virtual void run() {}
	virtual RModifiers prepare() { return RModifiers(); }
	virtual const Glib::ustring get_label() const = 0;
};

class Command : public Action {
public:
	std::string cmd;
	Command() = default;
    explicit Command(std::string c) : cmd(std::move(c)) {}
	virtual void run();
	virtual const Glib::ustring get_label() const { return cmd; }
};

class ModAction : public Action {
protected:
	ModAction() {}
	Gdk::ModifierType mods;
	ModAction(Gdk::ModifierType mods_) : mods(mods_) {}
	virtual RModifiers prepare();
public:
	virtual const Glib::ustring get_label() const;
};

class SendKey : public ModAction {
	guint key;
	SendKey(guint key_, Gdk::ModifierType mods) :
		ModAction(mods), key(key_) {}
public:
	SendKey() {}
	static RSendKey create(guint key, Gdk::ModifierType mods) {
		return RSendKey(new SendKey(key, mods));
	}

	virtual void run();
	virtual RModifiers prepare();
	virtual const Glib::ustring get_label() const;
};
#define IS_KEY(act) (act && dynamic_cast<SendKey *>(act.get()))

class SendText : public Action {
	Glib::ustring text;
	SendText(Glib::ustring text_) : text(text_) {}
public:
	SendText() {}
	static RSendText create(Glib::ustring text) { return RSendText(new SendText(text)); }

	virtual void run();
	virtual const Glib::ustring get_label() const { return text; }
};

class Scroll : public ModAction {
	Scroll(Gdk::ModifierType mods) : ModAction(mods) {}
public:
	Scroll() {}
	static RScroll create(Gdk::ModifierType mods) { return RScroll(new Scroll(mods)); }
	virtual const Glib::ustring get_label() const;
};
#define IS_SCROLL(act) (act && dynamic_cast<Scroll *>(act.get()))

class Ignore : public ModAction {
	Ignore(Gdk::ModifierType mods) : ModAction(mods) {}
public:
	Ignore() {}
	static RIgnore create(Gdk::ModifierType mods) { return RIgnore(new Ignore(mods)); }
	virtual const Glib::ustring get_label() const;
};
#define IS_IGNORE(act) (act && dynamic_cast<Ignore *>(act.get()))

class Button : public ModAction {
	Button(Gdk::ModifierType mods, guint button_) : ModAction(mods), button(button_) {}
	guint button;
public:
	Button() {}
	ButtonInfo get_button_info() const;
	static unsigned int get_button(RAction act) {
		if (!act)
			return 0;
		Button *b = dynamic_cast<Button *>(act.get());
		if (!b)
			return 0;
		return b->get_button_info().button;
	}
	static RButton create(Gdk::ModifierType mods, guint button_) { return RButton(new Button(mods, button_)); }
	virtual const Glib::ustring get_label() const;
	virtual void run();
};
#define IF_BUTTON(act, b) if (unsigned int b = Button::get_button(act))

class Misc : public Action {
public:
	enum Type { NONE, UNMINIMIZE, SHOWHIDE, DISABLE };
	Type type;
private:
	Misc(Type t) : type(t) {}
public:
	static const char *types[5];
	Misc() {}
	virtual const Glib::ustring get_label() const;
	static RMisc create(Type t) { return RMisc(new Misc(t)); }
	virtual void run();
};

class StrokeSet : public std::set<RStroke> {
};

// Internal use only
class Click : public Action {
	virtual const Glib::ustring get_label() const { return "Click"; }
};
#define IS_CLICK(act) (act && dynamic_cast<Click *>(act.get()))

class StrokeInfo {
public:
	StrokeInfo(RStroke s, RAction a) : action(a) { strokes.insert(s); }
	StrokeInfo() {}
	StrokeSet strokes;
	RAction action;
	std::string name;
};
typedef std::shared_ptr<StrokeInfo> RStrokeInfo;

class Ranking {
	static bool show(RRanking r);
	int x, y;
public:
	RStroke stroke, best_stroke;
	RAction action;
	double score;
	std::string name;
	std::multimap<double, std::pair<std::string, RStroke> > r;
	static void queue_show(RRanking r, RTriple e);
};

class Unique {
public:
	int level;
	int i;
};

class ActionListDiff {
	friend class ActionDB;
	ActionListDiff *parent;
	std::set<Unique *> deleted;
	std::map<Unique *, std::shared_ptr<StrokeInfo>> added;
	std::list<Unique *> order;
	std::list<ActionListDiff> children;

	void update_order() {
		int j = 0;
		for (auto i = order.begin(); i != order.end(); i++, j++) {
			(*i)->level = level;
			(*i)->i = j;
		}
	}

public:
	int level;
	bool app;
	std::string name;

	ActionListDiff() : parent(nullptr), level(0), app(false) {}

	typedef std::list<ActionListDiff>::iterator iterator;
	iterator begin() { return children.begin(); }
	iterator end() { return children.end(); }

	RStrokeInfo get_info(Unique *id, bool *deleted = 0, bool *stroke = 0, bool *name = 0, bool *action = 0) const;

	int size_rec() const {
		int size = added.size();
		for (auto i = children.begin(); i != children.end(); i++)
			size += i->size_rec();
		return size;
	}

	Unique *add(std::shared_ptr<StrokeInfo> si, Unique *before = 0) {
		auto *id = new Unique;
		added.insert(std::pair<Unique *, std::shared_ptr<StrokeInfo>>(id, si));
		id->level = level;
		id->i = order.size();
		if (before)
			order.insert(std::find(order.begin(), order.end(), before), id);
		else
			order.push_back(id);
		update_order();
		return id;
	}

	bool contains(Unique *id) const {
		if (deleted.count(id))
			return false;
		if (added.count(id))
			return true;
		return parent && parent->contains(id);
	}

	void add_apps(std::map<std::string, ActionListDiff *> &apps) {
		if (app)
			apps[name] = this;
		for (std::list<ActionListDiff>::iterator i = children.begin(); i != children.end(); i++)
			i->add_apps(apps);
	}

	std::shared_ptr<std::map<Unique *, StrokeSet> > get_strokes() const;
	std::shared_ptr<std::set<Unique *> > get_ids(bool include_deleted) const;

	int count_actions() const {
		return (parent ? parent->count_actions() : 0) + order.size() - deleted.size();
	}
	void all_strokes(std::list<RStroke> &strokes) const;
	RAction handle(RStroke s, RRanking &r) const;
	// b1 is always reported as b2
	void handle_advanced(RStroke s, std::map<guint, RAction> &a, std::map<guint, RRanking> &r, int b1, int b2) const;

	~ActionListDiff();
};

class ActionDB {
public:
	std::map<std::string, ActionListDiff *> apps;
private:
	ActionListDiff root;
public:
	typedef std::map<Unique *, std::shared_ptr<StrokeInfo>>::const_iterator const_iterator;
	const const_iterator begin() const { return root.added.begin(); }
	const const_iterator end() const { return root.added.end(); }

	const ActionListDiff *get_action_list(std::string wm_class) const {
		auto i = apps.find(wm_class);
		return i == apps.end() ? &root : i->second;
	}
	ActionDB();
};

extern ActionDB actions;
