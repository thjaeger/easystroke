#ifndef __VAR_H__
#define __VAR_H__

#include <set>
#include <glibmm/thread.h>

extern Glib::StaticRecMutex global_mutex;

struct Atomic {
	Atomic() { global_mutex.lock(); }
	~Atomic() { global_mutex.unlock(); }
};

template <class T> class Out;
template <class T> class Var;

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
	Out<T> *in;
public:
	In() : in(0) {}
};

template <class T> class Out : virtual public Base {
	std::set<In<T> *> out;
protected:
	virtual void update(Update<T> &u, Out<T> *exclude = 0) {
		for (typename std::set<In<T> *>::iterator i = out.begin(); i != out.end(); i++) {
			In<T> *cur = *i;
			if (!exclude || dynamic_cast<Base *>(cur) != dynamic_cast<Base *>(exclude))
				cur->notify(u, this);
		}
	}
	void update() {
		NewValue<T> u(get());
		update(u);
	}
public:
	virtual T get() = 0;
	void connectOut(In<T> *in) {
		Atomic a;
		out.insert(in);
		in->in = this;
	}
};

template <class T> class Var : public Out<T> {
	T v;
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
	T &write_ref(Atomic &a) { return v; }
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
#endif
