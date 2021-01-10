#pragma once
#include "gesture.h"
#include "grabber.h"
#include "actiondb.h"

class EventLoop;

class Handler {
public:
	Handler *child;
	Handler *parent;
	Handler() : child(nullptr), parent(nullptr) {}
	Handler *top() {
		if (child)
			return child->top();
		else
			return this;
	}

	virtual void motion(CursorPosition e) {}
	virtual void raw_motion(CursorPosition e, bool, bool) {}
	virtual void press(guint b, CursorPosition e) {}
	virtual void release(guint b, CursorPosition e) {}
	virtual void press_master(guint b, Time t) {}
	void replace_child(Handler *c);
	virtual void init() {}
	virtual ~Handler() {
		if (child)
			delete child;
	}
	virtual std::string name() = 0;
	virtual Grabber::State grab_mode() = 0;

	void runAction(const std::shared_ptr<Actions::Action>& act);
};

class HandlerFactory {
public:
    static std::unique_ptr<Handler> makeIdleHandler(EventLoop *xstate_);
};
