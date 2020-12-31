#include <glibmm/i18n.h>
#include <string>

#include "actiondb.h"
#include "win.h"

using namespace std;

void Actions::Command::run() {
	pid_t pid = fork();
	switch (pid) {
		case 0:
			execlp("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
			exit(1);
		case -1:
			g_warning("can't execute command \"%s\": fork() failed", cmd.c_str());
	}
}

ButtonInfo Actions::Button::get_button_info() const {
	ButtonInfo bi;
	bi.button = button;
	bi.state = mods;
	return bi;
}

const Glib::ustring Actions::Button::get_label() const {
	return get_button_info().get_button_text();
}

const Glib::ustring Actions::Misc::get_label() const { return types[type]; }

const char *Actions::Misc::types[5] = { "None", "Unminimize", "Show/Hide", "Disable (Enable)", nullptr };

ActionDB::ActionDB() {
	root.name = "Default";

	auto upStroke = PreStroke();
	upStroke.add(create_triple(1, 10, 0));
	upStroke.add(create_triple(1, 1, 1));
    upStroke.add(create_triple(1, 1, 1));

    auto leftStroke = PreStroke();
    leftStroke.add(create_triple(10, 1, 0));
    leftStroke.add(create_triple(1, 1, 1));
    leftStroke.add(create_triple(1, 1, 1));

    auto rightStroke = PreStroke();
    rightStroke.add(create_triple(1, 1, 0));
    rightStroke.add(create_triple(10, 1, 1));
    rightStroke.add(create_triple(10, 1, 1));

    root.add(std::make_shared<StrokeInfo>(
        std::make_shared<Stroke>(upStroke, 0, 0, 32768, false),
        std::make_shared<Actions::Command>("dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.PlayPause"))
    );

    root.add(std::make_shared<StrokeInfo>(
            std::make_shared<Stroke>(leftStroke, 0, 0, 32768, false),
            std::make_shared<Actions::Command>("dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.Previous"))
    );

    root.add(std::make_shared<StrokeInfo>(
            std::make_shared<Stroke>(rightStroke, 0, 0, 32768, false),
            std::make_shared<Actions::Command>("dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.Next"))
    );
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
	auto i = added.find(id);
	for (i = added.begin(); i != added.end(); i++) {
		if (i->first == id)
			break;
	}
	if (i == added.end()) {
		return si;
	}
	if (i->second->name != "") {
		si->name = i->second->name;
		if (name)
			*name = parent;
	}
	if (i->second->strokes.size()) {
		si->strokes = i->second->strokes;
		if (stroke)
			*stroke = parent;
	}
	if (i->second->action) {
		si->action = i->second->action;
		if (action)
			*action = parent;
	}
	return si;
}

std::shared_ptr<std::map<Unique *, StrokeSet> > ActionListDiff::get_strokes() const {
	std::shared_ptr<std::map<Unique *, StrokeSet> > strokes = parent ? parent->get_strokes() :
		std::shared_ptr<std::map<Unique *, StrokeSet> >(new std::map<Unique *, StrokeSet>);
	for (auto i = deleted.begin(); i != deleted.end(); i++)
		strokes->erase(*i);
	for (auto i = added.begin(); i != added.end(); i++)
		if (i->second->strokes.size())
			(*strokes)[i->first] = i->second->strokes;

	return strokes;
}

std::shared_ptr<std::set<Unique *> > ActionListDiff::get_ids(bool include_deleted) const {
	std::shared_ptr<std::set<Unique *> > ids = parent ? parent->get_ids(false) :
		std::shared_ptr<std::set<Unique *> >(new std::set<Unique *>);
	if (!include_deleted)
		for (auto i = deleted.begin(); i != deleted.end(); i++)
			ids->erase(*i);
	for (auto i = added.begin(); i != added.end(); i++)
		ids->insert(i->first);
	return ids;
}

void ActionListDiff::all_strokes(std::list<RStroke> &strokes) const {
	for (auto i = added.begin(); i != added.end(); i++)
		for (auto j = i->second->strokes.begin(); j != i->second->strokes.end(); j++)
			strokes.push_back(*j);
	for (auto i = children.begin(); i != children.end(); i++)
		i->all_strokes(strokes);
}

Actions::RAction ActionListDiff::handle(RStroke s, RRanking &r) const {
	if (!s)
		return Actions::RAction();
	r.reset(new Ranking);
	r->stroke = s;
	r->score = 0.0;
	auto strokes = get_strokes();
	for (auto i = strokes->begin(); i!=strokes->end(); i++) {
		for (auto j = i->second.begin(); j!=i->second.end(); j++) {
			double score;
			int match = Stroke::compare(s, *j, score);
			if (match < 0)
				continue;
			RStrokeInfo si = get_info(i->first);
			r->r.insert(pair<double, pair<std::string, RStroke> >
					(score, pair<std::string, RStroke>(si->name, *j)));
			if (score > r->score) {
				r->score = score;
				if (match) {
					r->name = si->name;
					r->action = si->action;
					r->best_stroke = *j;
				}
			}
		}
	}
	if (!r->action && s->trivial())
		return Actions::RAction(new Actions::Click);
	if (r->action) {
        g_message("Executing Action %s", r->name.c_str());
	} else {
        g_message("Couldn't find matching stroke.");
	}
	return r->action;
}

void ActionListDiff::handle_advanced(RStroke s, std::map<guint, Actions::RAction> &as,
		std::map<guint, RRanking> &rs, int b1, int b2) const {
	if (!s)
		return;
	std::shared_ptr<std::map<Unique *, StrokeSet> > strokes = get_strokes();
	for (auto i = strokes->begin(); i!=strokes->end(); i++) {
		for (auto j = i->second.begin(); j!=i->second.end(); j++) {
			int b = (*j)->button;
			if (!s->timeout && !b)
				continue;
			s->button = b;
			double score;
			int match = Stroke::compare(s, *j, score);
			if (match < 0)
				continue;
			Ranking *r;
			if (b == b1)
				b = b2;
			if (rs.count(b)) {
				r = rs[b].get();
			} else {
				r = new Ranking;
				rs[b].reset(r);
				r->stroke = RStroke(new Stroke(*s));
				r->score = -1;
			}
			RStrokeInfo si = get_info(i->first);
			r->r.insert(pair<double, pair<std::string, RStroke> >
					(score, pair<std::string, RStroke>(si->name, *j)));
			if (score > r->score) {
				r->score = score;
				if (match) {
					r->name = si->name;
					r->action = si->action;
					r->best_stroke = *j;
					as[b] = si->action;
				}
			}
		}
	}
}

ActionListDiff::~ActionListDiff() {
	if (app)
		actions.apps.erase(name);
}

ActionDB actions;
