#pragma once
#include <set>
#include <glibmm.h>
#include "util.h"

class Base {
public:
	virtual void notify() = 0;
	virtual ~Base() {}
};

template <class T> class Out {
	std::set<Base *> out;
protected:
	void update() {
		for (auto i : out)
			i->notify();
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
	explicit Source(T x_) : x(x_) {}

	virtual void set(const T x_) {
		x = x_;
		Out<T>::update();
	}

	virtual T get() const { return x; }

	virtual void notify() { Out<T>::update(); }
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
