#include <glibmm/i18n.h>
#include <memory>
#include <string>

#include "actiondb.h"

using namespace std;

ActionDB::ActionDB() {
    auto upStroke = std::vector<CursorPosition>();
    upStroke.emplace_back(1, 10, 0);
    upStroke.emplace_back(1, 1, 1);
    upStroke.emplace_back(1, 1, 1);

    auto leftStroke = std::vector<CursorPosition>();
    leftStroke.emplace_back(10, 1, 0);
    leftStroke.emplace_back(1, 1, 1);
    leftStroke.emplace_back(1, 1, 1);

    auto rightStroke = std::vector<CursorPosition>();
    rightStroke.emplace_back(1, 1, 0);
    rightStroke.emplace_back(10, 1, 1);
    rightStroke.emplace_back(10, 1, 1);

    global.push_back(std::make_shared<StrokeInfo>(
            std::make_shared<Gesture>(upStroke, 0, 0, 32768, false),
            std::make_shared<Actions::Command>(
                    "dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.PlayPause"))
    );

    global.push_back(std::make_shared<StrokeInfo>(
            std::make_shared<Gesture>(leftStroke, 0, 0, 32768, false),
            std::make_shared<Actions::Command>(
                    "dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.Previous"))
    );

    global.push_back(std::make_shared<StrokeInfo>(
            std::make_shared<Gesture>(rightStroke, 0, 0, 32768, false),
            std::make_shared<Actions::Command>(
                    "dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.Next"))
    );
}

std::shared_ptr<Actions::Action> ActionDB::handle(const Gesture& s, const std::string& application) {
	shared_ptr<StrokeInfo> winner = nullptr;
	auto winning_score = 0.0;

	for (const auto& i : global) {
        auto [match, score] = Gesture::compare(s, *i->gesture);
        if (!match) {
            continue;
        }

        if (score > winning_score) {
            winning_score = score;
            winner = i;
        }
	}

	if (!winner && s.trivial())
		return std::make_shared<Actions::Click>();
	if (winner) {
        g_message("Executing Action %s", winner->name.c_str());
        return winner->action;
	} else {
        g_message("Couldn't find matching stroke.");
        return nullptr;
	}
}

void ActionDB::handle_advanced(
    const Gesture& s, std::map<guint, std::shared_ptr<Actions::Action>> as, std::map<guint, double> rs,
    guint b1, guint b2, const std::string& application) {

    for (const auto& i : global) {
        int b = i->gesture->button;
        if (!s.timeout && !b) {
            continue;
        }

        auto[match, score] = Gesture::compareNoButton(s, *i->gesture);
        if (!match) {
            continue;
        }

        if (b == b1) {
            b = b2;
        }

        if (!rs.count(b)) {
            rs[b] = -1;
        }

        if (score > rs[b]) {
            rs[b] = score;
            as[b] = i->action;
        }
    }
}

ActionDB actions;
