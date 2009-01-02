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
#include "actiondb.h"
#include "main.h"
#include <glibmm/i18n.h>

#include <iostream>
#include <fstream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/shared_ptr.hpp>

#include <X11/extensions/XTest.h>

BOOST_CLASS_EXPORT(StrokeSet)

BOOST_CLASS_EXPORT(Action)

template<class Archive> void Unique::serialize(Archive & ar, const unsigned int version) {}

template<class Archive> void Action::serialize(Archive & ar, const unsigned int version) {}

template<class Archive> void StrokeSet::serialize(Archive & ar, const unsigned int version) {
	ar & boost::serialization::base_object<std::set<RStroke> >(*this);
}

template<class Archive> void StrokeInfo::serialize(Archive & ar, const unsigned int version) {
	ar & strokes;
	ar & action;
	if (version == 0) return;
	ar & name;
}

using namespace std;

template<class Archive> void ActionListDiff::serialize(Archive & ar, const unsigned int version) {
	ar & deleted;
	ar & added;
	ar & name;
	ar & children;
	ar & app;
	if (version == 0)
		return;
	ar & order;
}

template<class Archive> void ActionDB::load(Archive & ar, const unsigned int version) {
	root.fix_tree(version == 2);
	root.add_apps(apps);
	root.name = _("Default");
}

template<class Archive> void ActionDB::save(Archive & ar, const unsigned int version) const {
	ar & root;
}

Source<bool> action_dummy;

void update_actions() {
	action_dummy.set(false);
}

void ActionDBWatcher::init() {
}

void ActionDBWatcher::timeout() {
}


RStrokeInfo ActionListDiff::get_info(Unique *id, bool *deleted, bool *stroke, bool *name, bool *action) const {
	if (deleted)
		*deleted = this->deleted.count(id);
	if (stroke)
		*stroke = false;
	if (name)
		*name = false;
	if (action)
		*action = false;
	RStrokeInfo si = parent ? parent->get_info(id) : RStrokeInfo(new StrokeInfo);
	std::map<Unique *, StrokeInfo>::const_iterator i = added.find(id);
	for (i = added.begin(); i != added.end(); i++) {
		if (i->first == id)
			break;
	}
	if (i == added.end()) {
		return si;
	}
	if (i->second.name != "") {
		si->name = i->second.name;
		if (name)
			*name = parent;
	}
	if (i->second.strokes.size()) {
		si->strokes = i->second.strokes;
		if (stroke)
			*stroke = parent;
	}
	if (i->second.action) {
		si->action = i->second.action;
		if (action)
			*action = parent;
	}
	return si;
}

boost::shared_ptr<std::map<Unique *, StrokeSet> > ActionListDiff::get_strokes() const {
	boost::shared_ptr<std::map<Unique *, StrokeSet> > strokes = parent ? parent->get_strokes() :
	       	boost::shared_ptr<std::map<Unique *, StrokeSet> >(new std::map<Unique *, StrokeSet>);
	for (std::set<Unique *>::const_iterator i = deleted.begin(); i != deleted.end(); i++)
		strokes->erase(*i);
	for (std::map<Unique *, StrokeInfo>::const_iterator i = added.begin(); i != added.end(); i++)
		if (i->second.strokes.size())
			(*strokes)[i->first] = i->second.strokes;
	return strokes;
}

boost::shared_ptr<std::set<Unique *> > ActionListDiff::get_ids(bool include_deleted) const {
	boost::shared_ptr<std::set<Unique *> > ids = parent ? parent->get_ids(false) :
	       	boost::shared_ptr<std::set<Unique *> >(new std::set<Unique *>);
	if (!include_deleted)
		for (std::set<Unique *>::const_iterator i = deleted.begin(); i != deleted.end(); i++)
			ids->erase(*i);
	for (std::map<Unique *, StrokeInfo>::const_iterator i = added.begin(); i != added.end(); i++)
		ids->insert(i->first);
	return ids;
}

void ActionListDiff::all_strokes(std::list<RStroke> &strokes) const {
	for (std::map<Unique *, StrokeInfo>::const_iterator i = added.begin(); i != added.end(); i++)
		for (std::set<RStroke>::const_iterator j = i->second.strokes.begin(); j != i->second.strokes.end(); j++)
			strokes.push_back(*j);
	for (std::list<ActionListDiff>::const_iterator i = children.begin(); i != children.end(); i++)
		i->all_strokes(strokes);
}

RAction ActionListDiff::handle(RStroke s, Ranking &r) const {
	r.action = RAction(new Click);
	r.name = _("click (default)");
	return r.action;
}

void ActionListDiff::handle_advanced(RStroke s, std::map<int, RAction> &as, std::map<int, Ranking *> &rs, int b1, int b2) const {
	as[b1] = RAction(new Click);
}

ActionListDiff::~ActionListDiff() {
	if (app)
		actions.apps.erase(name);
}

ActionDB actions;
