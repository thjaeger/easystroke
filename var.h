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

#include <set>
#include <list>
#include <boost/shared_ptr.hpp>
#include <glibmm.h>

typedef unsigned long long int uint64;

class In {
	static uint64 count;
public:
	In() : index(count++) {}
	virtual void notify() = 0;
	const uint64 index;
};

void mark_dirty(In *);
void update_dirty();

struct Atomic {
	~Atomic() { update_dirty(); }
};

template <class T> class Out {
	std::set<In *> out;
protected:
	void update() {
		for (typename std::set<In *>::iterator i = out.begin(); i != out.end(); i++)
			mark_dirty(*i);
	}
public:
	void connect(In *o) { out.insert(o); }
	virtual T get() = 0;
};

template <class T> class IO : public Out<T> {
public:
	virtual void set(const T x_) = 0;
};

template <class T> class Source : public IO<T> {
	T x;
public:
	Source() {}
	Source(T x_) : x(x_) {}
	virtual void set(const T x_) { 
		x = x_;
		Out<T>::update();
		update_dirty();
	}
	virtual T get() { return x; }
	const T &ref() { return x; } // TODO should be shared w/ Var
	// write_refs are evil
	T &write_ref(Atomic &a) { Out<T>::update(); return x; }
	// unsafe_refs even more so
	T &unsafe_ref() { return x; }
};

template <class T> class Var : public Out<T>, public In {
	Out<T> &in;
	T x;
public:
	Var(Out<T> &in_) : in(in_), x(in.get()) { in.connect(this); }
	virtual T get() { return x; }
	virtual void notify() { 
		x = in.get();
		Out<T>::update();
	}
	const T &ref() { return x; }
};

template <class X, class Y> class Fun : public Out<Y>, public In {
	Out<X> &in;
protected:
	virtual Y run(const X &) = 0;
	Fun(Out<X> &in_) : in(in_) { in.connect(this); }
public:
	virtual Y get() { return run(in.get()); }
	virtual void notify() { Out<Y>::update(); }
};

template <class X, class Y> class Bijection : public IO<Y>, public In {
	IO<X> &io;
protected:
	virtual Y run(const X &) = 0;
	virtual X inverse(const Y &) = 0;
	Bijection(IO<X> &io_) : io(io_) { io.connect(this); }
public:
	virtual Y get() { return run(io.get()); }
	virtual void notify() { Out<Y>::update(); }
	virtual void set(const Y y) { io.set(inverse(y)); }
};

template <class X, class Y> class Converter : public Bijection<X, Y> {
protected:
	virtual Y run(const X &x) { return (Y)x; }
	virtual X inverse(const Y &y) { return (X)y; }
public:
	Converter(IO<X> &io_) : Bijection<X, Y>(io_) {}
};

class Watcher : public In {
public:
	template <class T> void watch(Out<T> &v) {
		v.connect(this);
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
};

#if 0
template <class T> class Collection : public Var<std::set<T *> > {
	typedef std::set<T *> C;
public:
	class Insert : public Update<C > {
		friend class Collection;
		T* x;
	public:
		virtual void apply(C &c) { c.insert(x); }
	};
	class Erase : public Update<C> {
		friend class Collection;
		T* x;
		virtual void apply(C &c) { c.erase(x); }
	};
	class Reference : public In<C> {
		friend class Collection;
		T *x;
		Reference(const Reference &);
		Reference& operator=(const Reference &);
		Reference(T *x_) : x(x_) {}
	public:
		virtual void notify(Update<C> &u, Out<C> *) {
			Erase *e = dynamic_cast<Erase *>(&u);
			if (e && e->x == x)
				x = 0;
		}
		bool valid() { return x != 0; }
	};
	typedef boost::shared_ptr<Reference> Ref;
	Ref insert(T *x) {
		Insert i;
		i.x = x;
		update(i);
		Ref ref = Ref(new Reference(x));
		connectOut(&*ref);
		return ref;
	}
	void erase(Ref r) {
		if (!r->valid())
			return;
		Erase e;
		e.x = r->x;
		update(e);
	}
};
#endif
#endif
