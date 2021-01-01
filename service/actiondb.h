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

class Ranking;
typedef std::shared_ptr<Ranking> RRanking;

class StrokeInfo {
public:
	StrokeInfo(std::shared_ptr<Stroke> s, std::shared_ptr<Actions::Action> a) : action(std::move(a)), stroke(std::move(s)) { }
	StrokeInfo() = default;
    std::shared_ptr<Stroke> stroke;
    std::shared_ptr<Actions::Action> action;
	std::string name;
};

class Ranking {
	int x;
public:
	double score;
	std::string name;
};

class ActionDB {
private:
    std::map<std::string, std::shared_ptr<std::vector<std::shared_ptr<StrokeInfo>>>> apps;
    std::vector<std::shared_ptr<StrokeInfo>> global;
public:
    ActionDB();

    std::shared_ptr<Actions::Action> handle(RStroke s, const std::string& application);

    void handle_advanced(RStroke sharedPtr, std::map<guint, std::shared_ptr<Actions::Action>> as, std::map<guint, RRanking> map1, guint i, guint i1, const std::string& application);

    bool disAllowApplication(const std::string& application) {
        return prefs.whitelist && !apps.count(application);
    }
};

extern ActionDB actions;
