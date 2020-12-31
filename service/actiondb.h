#pragma once
#include <string>
#include <map>
#include <set>
#include <iostream>
#include <utility>

#include "gesture.h"
#include "prefdb.h"

#include "actions/modifiers.h"
#include "actions/action.h"
#include "actions/modAction.h"

class Unique;

class Ranking;
typedef std::shared_ptr<Ranking> RRanking;


class StrokeSet : public std::set<RStroke> {
};

class StrokeInfo {
public:
	StrokeInfo(RStroke s, std::shared_ptr<Actions::Action> a) : action(a) { strokes.insert(s); }
	StrokeInfo() {}
	StrokeSet strokes;
    std::shared_ptr<Actions::Action> action;
	std::string name;
};
typedef std::shared_ptr<StrokeInfo> RStrokeInfo;

class Ranking {
	static bool show(RRanking r);
	int x, y;
public:
	RStroke stroke, best_stroke;
    std::shared_ptr<Actions::Action> action;
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
    std::shared_ptr<Actions::Action> handle(RStroke s, RRanking &r) const;
	// b1 is always reported as b2
	void handle_advanced(RStroke s, std::map<guint, std::shared_ptr<Actions::Action>> &a, std::map<guint, RRanking> &r, int b1, int b2) const;

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
