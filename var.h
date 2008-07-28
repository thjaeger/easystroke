#ifndef __VAR_H__
#define __VAR_H__

#include <set>
#include <glibmm/thread.h>

extern Glib::StaticRecMutex global_mutex;

class Atomic {
public:
	Atomic() { global_mutex.lock(); }
	~Atomic() { global_mutex.unlock(); }
};

template <class T> class Var;
template <class T> class VarE;
template <class T> class VarI;

template <class T> class In {
	friend class Var<T>;
protected:
	Var<T> *parent;
public:
	In() : parent(0) {}
	virtual void notify(T &t) = 0;
	virtual ~In() {
		if (parent)
			parent->out.erase(this);
	}
};

template <class T> class Out {
	friend class VarE<T>;
protected:
	VarE<T> *parent;
	void set(T x) {
		if (parent) {
			parent->v = x;
			parent->update();
		}
	}
public:
	Out() : parent(0) {}
	virtual ~Out() {
		if (parent)
			parent->in = 0;
	}
};

template <class T> class IO : public In<T> {
protected:
	void set(T x) {
		if (In<T>::parent) {
			In<T>::parent->v = x;
			In<T>::parent->update(this);
		}
	}
};

template <class X, class Y> class Fun : public In<X>, public Out<Y> {
public:
	virtual Y run(X &x) = 0;
	virtual void notify(X &x) { Out<Y>::set(run(x)); }
};

#define XY(X,Y) X##Y
#define MakeNameXY(FX,LINE) XY(FX,LINE)
#define MakeName MakeNameXY(Foo,__LINE__)

#define FUN(YT,Y,XT,X,F) class MakeName : public Fun<XT,YT> { virtual YT run(XT &X) { return F; } }; Fun<XT,YT> *Y = new MakeName;

template <class T> class IO1 : public IO<T> {
protected:
	virtual void notify1(T &x) = 0;
public:
	virtual void notify(T &x) { notify1(x); }
};

template <class T> class IO2 : public IO<T> {
protected:
	virtual void notify2(T &x) = 0;
public:
	virtual void notify(T &x) { notify2(x); }
};

template <class X, class Y> class BiFun : public IO1<X>, public IO2<Y> {
public:
	virtual Y run1(X &x) = 0;
	virtual X run2(Y &y) = 0;
	virtual void notify1(X &x) { IO2<Y>::set(run1(x)); }
	virtual void notify2(Y &y) { IO1<X>::set(run2(y)); }
};

template <class T> class Var {
	friend class Setter;
	friend class In<T>;
	friend class IO<T>;
protected:
	std::set<In<T> *> out;
	T v;
	void update(In<T> *exclude = 0) {
		for (typename std::set<In<T> *>::iterator i = out.begin(); i != out.end(); i++) {
			In<T> *cur = *i;
			if (cur != exclude)
				cur->notify(v);
		}
	}
public:
	Var(T v_) : v(v_) {}
	Var() {}
	const T get() {
		Atomic a;
		return v;
	}
	void connect(In<T> *f, bool notify) {
		Atomic a;
		out.insert(f);
		f->parent = this;
		if (notify)
			f->notify(v);
	}
};

template <class T> class VarE : public Var<T> {
	friend class Out<T>;
	Out<T> *in;
public:
	VarE(T v) : Var<T>(v), in(0) {}
	VarE() : Var<T>(), in(0) {}
	void freeze() { delete in; }
	void connectE(Out<T> *f) {
		Atomic a;
		in = f;
		f->parent = this;
	}
	void set(T x) {
		Atomic a;
		freeze();
		Var<T>::v = x;
		Var<T>::update();
	}
	template <class X> void assign(Fun<X, T> *f, Var<X> &x) {
		Atomic a;
		freeze();
		connectE(f);
		x.connect(f, true);
	}
	void assign(Var<T> &x) {
		class Identity : public Fun<T,T> {
			T run(T &x) { return x; }
		};
		assign(new Identity, x);
	}
};

template <class T> class VarI : public Var<T> {
public:
	VarI(T v) : Var<T>(v) {}
	VarI() : Var<T>() {}
	void set(T x) {
		Atomic a;
		Var<T>::v = x;
		Var<T>::update();
	}
	template <class X> void identify(BiFun<X, T> *f, VarI<X> &x) {
		Atomic a;
		IO2<T> *f2 = f;
		connect(f2, false);
		IO1<X> *f1 = f;
		x.connect(f1, true);
	}
	void identify(VarI<T> &x) {
		class BiIdentity : public BiFun<T,T> {
			T run1(T &x) { return x; }
			T run2(T &x) { return x; }
		};
		identify(new BiIdentity, x);
	}
};

class Setter : public Atomic {
public:
	template <class T> const T &ref(Var<T> &v) { return v.v; }
	// write_refs are evil
	template <class T> T &write_ref(Var<T> &v) { return v.v; }
};
#endif
