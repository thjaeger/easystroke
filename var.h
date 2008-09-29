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
#ifndef __VAR_H__
#define __VAR_H__

#include <list>
#include <boost/shared_ptr.hpp>
#include <glibmm.h>

class Atomic {
	typedef sigc::slot<void> Update;
	std::list<Update> update_queue;
public:
	void defer(Update f) { update_queue.push_back(f); }
	~Atomic() {
		for (std::list<Update>::iterator i = update_queue.begin(); i != update_queue.end(); i++)
			(*i)();
	}
};

// Think of T as a type of morphisms
template <class T> class Out {
	typedef sigc::slot<void, const T> Notifier;
	std::list<Notifier> out;
protected:
	void update(const T v) {
		for (typename std::list<Notifier>::iterator i = out.begin(); i != out.end(); i++) {
			(*i)(v);
		}
	}
public:
	void connect(Notifier o) { out.push_back(o); }
};

template <class T> class Value : public Out<T> {
	public:
	virtual T get() = 0;
};

template <class T> class IO : public Value<T> {
public:
	virtual void set(const T) = 0;
};

template <class T> class Source : public IO<T> {
	T x;
	void do_update() { Out<T>::update(x); }
public:
	Source() {}
	Source(T x_) : x(x_) {}
	virtual void set(const T x_) {
		x = x_;
		Out<T>::update(x);
	}
	virtual T get() { return x; }
	const T &ref() { return x; } // TODO should be shared w/ Var
	// write_refs are evil
	T &write_ref(Atomic &a) {
		a.defer(sigc::mem_fun(*this, &Source::do_update));
		return x;
	}
	// unsafe_refs even more so
	T &unsafe_ref() { return x; }
};

// one input
template <class T> class In {
	virtual void notify(const T) = 0;
public:
	In(Out<T> &in) { in.connect(sigc::mem_fun(*this, &In::notify)); }
};

template <class T> class Var : public Value<T>, public In<T> {
	T x;
public:
	Var(Value<T> &in) : In<T>(in), x(in.get()) {}
	virtual T get() { return x; }
	virtual void notify(const T x_) {
		x = x_;
		Out<T>::update(x);
	}
	const T &ref() { return x; }
};

template <class X, class Y> class Fun : public Value<Y>, public In<X> {
	Value<X> &in;
protected:
	virtual Y run(const X &) = 0;
	Fun(Value<X> &in_) : In<X>(in_), in(in_) {}
public:
	virtual Y get() { return run(in.get()); }
	virtual void notify(const X x) { Out<Y>::update(run(x)); }
};

template <class X, class Y> class Bijection : public IO<Y>, public In<X> {
	IO<X> &io;
protected:
	virtual Y run(const X &) = 0;
	virtual X inverse(const Y &) = 0;
	Bijection(IO<X> &io_) : In<X>(io_), io(io_) {}
public:
	virtual Y get() { return run(io.get()); }
	virtual void notify(const X x) { Out<Y>::update(run(x)); }
	virtual void set(const Y y) { io.set(inverse(y)); }
};

template <class X, class Y> class Converter : public Bijection<X, Y> {
protected:
	virtual Y run(const X &x) { return (Y)x; }
	virtual X inverse(const Y &y) { return (X)y; }
public:
	Converter(IO<X> &io_) : Bijection<X, Y>(io_) {}
};

class Watcher {
	template <class T> void notify_priv(const T) { notify(); }
protected:
	virtual void notify() = 0;

public:
	template <class T> void watch(Out<T> &v) {
		v.connect(sigc::mem_fun(*this, &Watcher::notify_priv<T>));
	}
};

class TimeoutWatcher : public Watcher {
	int ms;
	sigc::connection *c;
	bool to() { timeout(); c = 0; return false; }
protected:
	virtual void timeout() = 0;
public:
	TimeoutWatcher(int ms_) : ms(ms_), c(0) {}
	virtual void notify() {
		if (c) {
			c->disconnect();
			delete c;
			c = 0;
		}
		c = new sigc::connection(Glib::signal_timeout().connect(sigc::mem_fun(*this, &TimeoutWatcher::to), ms));
	}
	void execute_now() {
		if (c) {
			c->disconnect();
			delete c;
			c = 0;
			timeout();
		}
	}
};
#endif
