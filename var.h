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

struct Base {};

template <class T> class In : virtual public Base {
	friend class Out<T>;
	friend class Var<T>;
	virtual void notify(T &, Out<T> *exclude) = 0;
};

template <class T> class Out : virtual public Base {
	std::set<In<T> *> out;
protected:
	virtual void update(T x, Out<T> *exclude = 0) {
		for (typename std::set<In<T> *>::iterator i = out.begin(); i != out.end(); i++) {
			In<T> *cur = *i;
			if (!exclude || dynamic_cast<Base *>(cur) != dynamic_cast<Base *>(exclude))
				cur->notify(x, this);
		}
	}
public:
	void connectOut(In<T> *in) {
		Atomic a;
		out.insert(in);
	}
};

template <class T> class Var : public Out<T> {
	friend class Setter;
	T v;
protected:
	virtual void update(T x, Out<T> *exclude = 0) {
		v = x;
		Out<T>::update(x, exclude);
	}
public:
	Var(T v_) : v(v_) {}
	Var() {}
	const T get() {
		Atomic a;
		return v;
	}
	void connect(In<T> *in) {
		Atomic a;
		connectOut(in);
		in->notify(v, this);
	}
};

template <class T> class IO : public In<T>, public Out<T> {};

template <class X, class Y> class Fun : public In<X>, public Out<Y> {
public:
	virtual Y run(X &x) = 0;
	virtual void notify(X &x, Out<X> *) { Out<Y>::update(run(x)); }
};

template <class X, class Y> class Fun1 : public Fun<X, Y> {
protected:
	virtual Y run1(X &x) = 0;
public:
	virtual Y run(X &x) { return run1(x); }
};

template <class X, class Y> class Fun2 : public Fun<X, Y> {
protected:
	virtual Y run2(X &x) = 0;
public:
	virtual Y run(X &x) { return run2(x); }
};

template <class X, class Y> class BiFun : public Fun1<X, Y>, public Fun2<Y, X> {
};

template <class T> class VarE : public Var<T>, public In<T> {
	virtual void notify(T &x, Out<T> *exclude) { update(x, exclude); }
public:
	VarE(T v) : Var<T>(v) {}
	VarE() : Var<T>() {}
	void set(T x) { update(x); }
	template <class X> void assign(Fun<X, T> *f, Var<X> &x) {
		Atomic a;
		f->connectOut(this);
		x.connect(f);
	}
	void assign(Var<T> &x) {
		x.connect(this);
	}
};

template <class T> class VarI : public Var<T>, public In<T> {
	virtual void notify(T &x, Out<T> *exclude) { update(x, exclude); }
public:
	VarI(T v) : Var<T>(v) {}
	VarI() : Var<T>() {}
	void set(T x) { update(x); }
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

struct Setter : public Atomic {
	template <class T> const T &ref(Var<T> &v) { return v.v; }
	// write_refs are evil
	template <class T> T &write_ref(Var<T> &v) { return v.v; }
};
#endif
