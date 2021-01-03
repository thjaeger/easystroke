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

class StrokeInfo {
public:
	StrokeInfo(std::shared_ptr<Gesture> g, std::shared_ptr<Actions::Action> a) : action(std::move(a)), gesture(std::move(g)) { }
	StrokeInfo() = default;
    std::shared_ptr<Gesture> gesture;
    std::shared_ptr<Actions::Action> action;
	std::string name;
};

class Ranking {
public:
	double score;
};

class ActionDB {
private:
    std::map<std::string, std::shared_ptr<std::vector<std::shared_ptr<StrokeInfo>>>> apps;
    std::vector<std::shared_ptr<StrokeInfo>> global;
public:
    ActionDB();

    std::shared_ptr<Actions::Action> handle(const Gesture &s, const std::string &application);

    void handle_advanced(
            const Gesture &s, std::map<guint, std::shared_ptr<Actions::Action>> as, std::map<guint, double> map1,
            guint i, guint i1, const std::string &application);

    bool disAllowApplication(const std::string &application) {
        return prefs.whitelist && !apps.count(application);
    }
};

extern ActionDB actions;
