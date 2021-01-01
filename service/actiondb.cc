#include <glibmm/i18n.h>
#include <memory>
#include <string>

#include "actiondb.h"

using namespace std;

ActionDB::ActionDB() {
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

    global.push_back(std::make_shared<StrokeInfo>(
        std::make_shared<Stroke>(upStroke, 0, 0, 32768, false),
        std::make_shared<Actions::Command>("dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.PlayPause"))
    );

    global.push_back(std::make_shared<StrokeInfo>(
            std::make_shared<Stroke>(leftStroke, 0, 0, 32768, false),
            std::make_shared<Actions::Command>("dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.Previous"))
    );

    global.push_back(std::make_shared<StrokeInfo>(
            std::make_shared<Stroke>(rightStroke, 0, 0, 32768, false),
            std::make_shared<Actions::Command>("dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.Next"))
    );
}

std::shared_ptr<Actions::Action> ActionDB::handle(RStroke s, const std::string& application) {
	if (!s) {
        g_error("s is undefined");
    }

	shared_ptr<StrokeInfo> winner = nullptr;
	auto winning_score = 0.0;

	for (const auto& i : global) {
        double score;
        int match = Stroke::compare(s, i->stroke, score);
        if (match < 0) {
            continue;
        }

        if (score > winning_score) {
            winning_score = score;
            if (match) {
                winner = i;
            }
        }
	}

	if (!winner && s->trivial())
		return std::make_shared<Actions::Click>();
	if (winner) {
        g_message("Executing Action %s", winner->name.c_str());
        return winner->action;
	} else {
        g_message("Couldn't find matching stroke.");
        return nullptr;
	}
}

void ActionDB::handle_advanced(RStroke s, std::map<guint, std::shared_ptr<Actions::Action>> as, std::map<guint, RRanking> rs, guint b1, guint b2, const std::string& application) {
	if (!s) {
        g_error("s is undefined");
    }

	for (auto i : global) {
        int b = i->stroke->button;
        if (!s->timeout && !b)
            continue;
        s->button = b;
        double score;
        int match = Stroke::compare(s, i->stroke, score);
        if (match < 0)
        {
            continue;
        }

        shared_ptr<Ranking> r;
        if (b == b1)
            b = b2;
        if (rs.count(b)) {
            r = rs[b];
        } else {
            r = make_shared<Ranking>();
            rs[b] = r;
            r->score = -1;
        }
        if (score > r->score) {
            r->score = score;
            if (match) {
                r->name = i->name;
                as[b] = i->action;
            }
        }
    }
}

ActionDB actions;
