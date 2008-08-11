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

template <class T> class Out;
template <class T> class Var;

struct Atomic {
	std::list<sigc::slot<void> > cleanup;
	~Atomic() {
		for (std::list<sigc::slot<void> >::iterator i = cleanup.begin(); i != cleanup.end(); i++)
			(*i)();
		cleanup.clear();
	}
};

template <class T> class Update {
public:
	virtual void apply(T &) = 0;
};

template <class T> class NewValue : public Update<T> {
	T v;
public:
	virtual void apply(T &v_) { v_ = v; }
	NewValue(T v_) : v(v_) {}
};

class Base {};

template <class T> class In : virtual public Base {
	friend class Out<T>;
	friend class Var<T>;
	virtual void notify(Update<T> &, Out<T> *exclude) = 0;
protected:
	Out<T> *in; // TODO: Doesn't really make sense for Vars
public:
	In() : in(0) {}
	virtual ~In() { if (in) in->out.erase(this); }
};

template <class T> class Out : virtual public Base {
	friend class In<T>;
	std::set<In<T> *> out;
protected:
	virtual void update(Update<T> &u, Out<T> *exclude = 0) {
		for (typename std::set<In<T> *>::iterator i = out.begin(); i != out.end(); i++) {
			In<T> *cur = *i;
			if (!exclude || dynamic_cast<Base *>(cur) != dynamic_cast<Base *>(exclude))
				cur->notify(u, this);
		}
	}
	void update(Out<T> *exclude = 0) {
		NewValue<T> u(get());
		update(u, exclude);
	}
public:
	virtual T get() = 0;
	void connectOut(In<T> *in) {
		Atomic a;
		out.insert(in);
		in->in = this;
	}
	virtual ~Out() {
		for (typename std::set<In<T> *>::iterator i = out.begin(); i != out.end(); i++) {
			In<T> *cur = *i;
			cur->in = 0;
		}
	}
};

template <class T> class Var : public Out<T> {
	T v;
	void cleanup() { Out<T>::update(); }
protected:
	virtual void update(Update<T> &u, Out<T> *exclude = 0) {
		Atomic a;
		u.apply(v);
		Out<T>::update(u, exclude);
	}
public:
	Var(T v_) : v(v_) {}
	Var() {}
	virtual T get() {
		Atomic a;
		return v;
	}
	void connect(In<T> *in) {
		Atomic a;
		connectOut(in);
		NewValue<T> u(v);
		in->notify(u, this);
	}
	const T &ref(Atomic &a) { return v; }
	// write_refs are evil
	T &write_ref(Atomic &a) {
		a.cleanup.push_back(sigc::mem_fun(*this, &Var::cleanup));
		return v;
	}
	// unsafe_refs even more so
	T &unsafe_ref() { return v; }
};

template <class X, class Y> class Fun : public In<X>, public Out<Y> {
public:
	virtual Y run(const X &x) = 0;
	virtual void notify(Update<X> &, Out<X> *) { Out<Y>::update(); }
	virtual Y get() { return run(In<X>::in->get()); }
};

template <class X, class Y> class Fun1 : public Fun<X, Y> {
protected:
	virtual Y run1(const X &x) = 0;
public:
	virtual Y run(const X &x) { return run1(x); }
};

template <class X, class Y> class Fun2 : public Fun<X, Y> {
protected:
	virtual Y run2(const X &x) = 0;
public:
	virtual Y run(const X &x) { return run2(x); }
};

template <class X, class Y> class BiFun : public Fun1<X, Y>, public Fun2<Y, X> {
};

template <class T> class VarE : public Var<T>, public In<T> {
	virtual void notify(Update<T> &u, Out<T> *exclude) { update(u, exclude); }
public:
	VarE(T v) : Var<T>(v) {}
	VarE() : Var<T>() {}
	void set(T x) { NewValue<T> u(x); update(u); }
	template <class X> void assign(Fun<X, T> *f, Var<X> &x) {
		Atomic a;
		f->connectOut(this);
		x.connect(f);
	}
	void assign(Var<T> &x) {
		x.connect(this);
	}
};


template <class T> class Interface : public In<T> {
	virtual void notify(Update<T> &, Out<T> *) { notify(In<T>::in->get()); }
protected:
	virtual void notify(T) = 0;
};

template <class T> class IO : public Interface<T>, public Out<T> {};

template <class T> class VarI : public Var<T>, public In<T> {
	virtual void notify(Update<T> &u, Out<T> *exclude) { update(u, exclude); }
public:
	VarI(T v) : Var<T>(v) {}
	VarI() : Var<T>() {}
	void set(T x) { NewValue<T> u(x); update(u); }
	template <class X> void identify(BiFun<X, T> *f, VarI<X> &x) {
		Atomic a;
		Fun2<T, X> *f2 = f;
		f2->connectOut(&x);
		connectOut(f2);
		Fun1<X, T> *f1 = f;
		f1->connectOut(this);
		x.connect(f1);
	}
	void identify(VarI<T> &x) {
		Atomic a;
		connectOut(&x);
		x.connect(this);
	}
	void identify(IO<T> *io) {
		Atomic a;
		io->connectOut(this);
		connect(io);
	}
};

class Watcher {
protected:
	virtual void notify() = 0;
public:
	template <class T> void watch(Out<T> &v) {
		class Watching : public In<T> {
			Watcher *w;
		public:
			Watching(Watcher *w_) : w(w_) {}
			virtual void notify(Update<T> &, Out<T> *exclude) { w->notify(); }
		};
		v.connectOut(new Watching(this));
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

#ifdef TEST_VAR
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
