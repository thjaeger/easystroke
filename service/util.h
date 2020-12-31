#pragma once
#include <glibmm.h>

class Timeout {
	// Invariant: c == &connection || c == nullptr
	sigc::connection *c;
	sigc::connection connection;
	// We have to account for the possibilty that timeout() destroys the object
	bool to() { c = nullptr; timeout(); return false; }
public:
	Timeout() : c(0) {}
protected:
	virtual void timeout() = 0;
public:
	bool remove_timeout() {
		if (c) {
			c->disconnect();
			c = 0;
			return true;
		}
		return false;
	}
	void set_timeout(int ms) {
		remove_timeout();
		connection = Glib::signal_timeout().connect(sigc::mem_fun(*this, &Timeout::to), ms);
		c = &connection;
	}
	virtual ~Timeout() {
		remove_timeout();
	}
};
