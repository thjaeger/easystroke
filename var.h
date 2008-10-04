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

template <class S> struct SetterBase {
	virtual void operator()(S *s) = 0;
};

template <class S> struct Setter : boost::shared_ptr<SetterBase<S> > {
	virtual void operator()(S *s) { (*(*this))(s); }
	Setter(SetterBase<S> *s) : boost::shared_ptr<SetterBase<S> >(s) {}
};

template <class S> class Setter0 : public SetterBase<S> {
	void (S::*f)();
public:
	Setter0(void (S::*f_)()) : f(f_) {}
	virtual void operator()(S *s) { (s->*f)(); }
};
template <class S, class T> Setter<S> setter0(void (S::*f)()) {
	return Setter<S>(new Setter0<S>(f));
}

template <class S, class T> class Setter1 : public SetterBase<S> {
	void (S::*f)(T);
	T x;
public:
	Setter1(void (S::*f_)(T), T x_) : f(f_), x(x_) {}
	virtual void operator()(S *s) { (s->*f)(x); }
};
template <class S, class T> Setter<S> setter1(void (S::*f)(T), T x) {
	return Setter<S>(new Setter1<S, T>(f, x));
}

template <class S, class T1, class T2> class Setter2 : public SetterBase<S> {
	void (S::*f)(T1, T2);
	T1 x1;
	T2 x2;
public:
	Setter2(void (S::*f_)(T1, T2), T1 x1_, T2 x2_) : f(f_), x1(x1_), x2(x2_) {}
	virtual void operator()(S *s) { (s->*f)(x1, x2); }
};
template <class S, class T1, class T2> Setter<S> setter2(void (S::*f)(T1, T2), T1 x1, T2 x2) {
	return Setter<S>(new Setter2<S, T1, T2>(f, x1, x2));
}


template <class G, class S> class Out : public G {
	std::list<S *> out;
protected:
	void foreach(Setter<S> f) { for_each(out.begin(), out.end(), f); }
public:
	void connect(S *s) { out.push_back(s); }
};

template <class T> struct ValueG {
		virtual T get() = 0;
};

template <class T> struct ValueS {
	virtual void set(const T) = 0;
};

template <class T> class ValueOut : public Out<ValueG<T>, ValueS<T> > {
protected:
	void do_set(T x) { foreach(setter1(&ValueS<T>::set, x)); }
};

template <class T> class ValueIO : public ValueOut<T>, public ValueS<T> {};

template <class T> class ValueProxy : public ValueS<T> {
	sigc::slot<void, const T> set_slot;
public:
	ValueProxy(sigc::slot<void, const T> set_slot_) : set_slot(set_slot_) {}
	virtual void set(const T x) { return set_slot(x); }
};

template <class T> class Source : public ValueIO<T> {
	T x;
	void update() { do_set(x); }
public:
	Source() {}
	Source(T x_) : x(x_) {}
	virtual void set(const T x_) {
		x = x_;
		update();
	}
	virtual T get() { return x; }
	const T &ref() { return x; }
	// write_refs are evil
	T &write_ref(Atomic &a) {
		a.defer(sigc::mem_fun(*this, &Source::update));
		return x;
	}
	// unsafe_refs even more so
	T &unsafe_ref() { return x; }
};

template <class T> class Var : public ValueOut<T>, private ValueS<T> {
	T x;
public:
	Var(ValueOut<T> &in) : x(in.get()) { in.connect(this); }
	virtual void set(const T x_) {
		x = x_;
		do_set(x);
	}
	virtual T get() { return x; }
};

template <class X, class Y> class Fun : public ValueOut<Y>, private ValueS<X> {
	sigc::slot<Y, X> f;
	ValueOut<X> &in;
public:
	Fun(sigc::slot<Y, X> f_, ValueOut<X> &in_) : f(f_), in(in_) { in.connect(this); }
	virtual Y get() { return f(in.get()); }
	virtual void set(const X x) {
		do_set(f(x));
	}
};

template <class X, class Y> Fun<X, Y> *fun(Y (*f)(X), ValueOut<X> &in) {
	return new Fun<X, Y>(sigc::ptr_fun(f), in);
}

template <class X, class Y, class Z> class Fun2 : public ValueOut<Z> {
	sigc::slot<Z, X, Y> f;
	ValueOut<X> &inX;
	ValueOut<Y> &inY;
public:
	Fun2(sigc::slot<Z, X, Y> f_, ValueOut<X> &inX_, ValueOut<Y> &inY_) : f(f_), inX(inX_), inY(inY_) {
		inX.connect(new ValueProxy<X>(sigc::mem_fun(*this, &Fun2::setX)));
		inY.connect(new ValueProxy<Y>(sigc::mem_fun(*this, &Fun2::setY)));
	}
	virtual Z get() { return f(inX.get(), inY.get()); }
	virtual void setX(const X x) { do_set(f(x, inY.get())); }
	virtual void setY(const Y y) { do_set(f(inX.get(), y)); }
};

template <class X1, class X2, class Y> Fun2<X1, X2, Y> *fun2(Y (*f)(X1, X2), ValueOut<X1> &in1, ValueOut<X2> &in2) {
	return new Fun2<X1, X2, Y>(sigc::ptr_fun(f), in1, in2);
}

template <class X, class Y> class Bijection : public ValueIO<Y> {
	sigc::slot<Y, X> f;
	sigc::slot<X, Y> g;
	ValueIO<X> &in;
public:
	Bijection(sigc::slot<Y, X> f_, sigc::slot<X, Y> g_, ValueIO<X> &in_) : f(f_), g(g_), in(in_) {
		in.connect(new ValueProxy<X>(sigc::mem_fun(*this, &Bijection::notify)));
	}
	virtual Y get() { return f(in.get()); }
	virtual void notify(const X x) { do_set(f(x)); }
	virtual void set(const Y y) { in.set(g(y)); }
};

template <class X, class Y> Y convert(X x) { return (Y)x; }
template <class X, class Y> Bijection<X, Y> *converter(ValueIO<X> &in) {
	return new Bijection<X, Y>(sigc::ptr_fun(&convert<X, Y>), sigc::ptr_fun(&convert<Y, X>), in);
}

class Watcher {
	template <class T> void notify_set(const T) { notify(); }
protected:
	virtual void notify() = 0;

public:
	template <class T> void watchValue(ValueOut<T> &v) {
		v.connect(new ValueProxy<T>(sigc::mem_fun(*this, &Watcher::notify_set<T>)));
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
