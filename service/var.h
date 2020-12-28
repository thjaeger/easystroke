/*
 * Copyright (c) 2008-2009, Thomas Jaeger <ThJaeger@gmail.com>
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
#ifndef __VAR_H__
#define __VAR_H__

#include <set>
#include <boost/shared_ptr.hpp>
#include <glibmm.h>
#include "util.h"

class Base {
public:
	virtual void notify() = 0;
	virtual ~Base() {}
};

class Notifier : public Base {
	sigc::slot<void> f;
public:
	Notifier(sigc::slot<void> f_) : f(f_) {}
	virtual void notify() { f(); }
};

class Atomic {
	std::set<Base *> update_queue;
public:
	void defer(Base *out) { update_queue.insert(out); }
	~Atomic() {
		for (std::set<Base *>::iterator i = update_queue.begin(); i != update_queue.end(); i++)
			(*i)->notify();
	}
};

template <class T> class Out {
	std::set<Base *> out;
protected:
	void update() {
		for (std::set<Base *>::iterator i = out.begin(); i != out.end(); i++)
			(*i)->notify();
	}
public:
	void connect(Base *s) { out.insert(s); }
	virtual T get() const = 0;
	virtual ~Out() {}
};

template <class T> class In {
public:
	virtual void set(const T x) = 0;
	virtual ~In() {}
};

template <class T> class IO : public In<T>, public Out<T> {};

template <class T> class Source : public IO<T>, private Base {
	T x;
public:
	Source() {}
	Source(T x_) : x(x_) {}
	virtual void set(const T x_) {
		x = x_;
		Out<T>::update();
	}
	virtual T get() const { return x; }
	const T &ref() const { return x; }
	// write_refs are evil
	T &write_ref(Atomic &a) {
		a.defer(this);
		return x;
	}
	virtual void notify() { Out<T>::update(); }
	// unsafe_refs even more so
	T &unsafe_ref() { return x; }
};

template <class T> class Var : public IO<T>, private Base {
	Out<T> &in;
	T x;
public:
	Var(Out<T> &in_) : in(in_), x(in.get()) { in.connect(this); }
	virtual void notify() { set(in.get()); }
	virtual void set(const T x_) {
		x = x_;
		Out<T>::update();
	}
	virtual T get() const { return x; }
};

template <class X, class Y> class Fun : public Out<Y>, private Base {
	sigc::slot<Y, X> f;
	Out<X> &in;
public:
	Fun(sigc::slot<Y, X> f_, Out<X> &in_) : f(f_), in(in_) { in.connect(this); }
	virtual Y get() const { return f(in.get()); }
	virtual void notify() { Out<Y>::update(); }
};

template <class X, class Y> Fun<X, Y> *fun(Y (*f)(X), Out<X> &in) {
	return new Fun<X, Y>(sigc::ptr_fun(f), in);
}

class Watcher : private Base {
public:
	template <class T> void watch(Out<T> &v) { v.connect(this); }
};

class TimeoutWatcher : public Watcher, Timeout {
	int ms;
public:
	TimeoutWatcher(int ms_) : ms(ms_) {}
	virtual void notify() { set_timeout(ms); }
	void execute_now() {
		if (remove_timeout())
			timeout();
	}
};
#endif
