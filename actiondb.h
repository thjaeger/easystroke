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
#include <iostream>

#include "prefdb.h"

class Action;

typedef boost::shared_ptr<Action> RAction;

class Unique;

class Modifiers;
typedef boost::shared_ptr<Modifiers> RModifiers;

class Action {
	friend std::ostream& operator<<(std::ostream& output, const Action& c);
public:
	virtual void run() {};
	virtual RModifiers prepare() { return RModifiers(); };
};

// Internal use only
class Click : public Action {
};
#define IS_CLICK(act) (act && dynamic_cast<Click *>(act.get()))

struct Ranking {
	RAction action;
	double score;
	Unique *id;
	std::string name;
	int x, y;
};

class Unique {
public:
	int level;
	int i;
};

class ActionListDiff {
public:
	RAction handle(Ranking &r) const;
	void handle_advanced(std::map<int, RAction> &a, int b1, int b2) const;
};
BOOST_CLASS_VERSION(ActionListDiff, 1)

class ActionDB {
	friend class ActionDBWatcher;

public:
	std::map<std::string, ActionListDiff *> apps;
private:
	ActionListDiff root;
public:
	ActionListDiff *get_root() { return &root; }

	const ActionListDiff *get_action_list(std::string wm_class) const {
		return &root;
	}
};
BOOST_CLASS_VERSION(ActionDB, 3)

extern ActionDB actions;
void update_actions();
#endif
