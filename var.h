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

template <class T> class In;
template <class T> class Out;
template <class T> class IO;
template <class T> class Var;
template <class T> class VarE;
template <class T> class VarI;

template <class T> class Var {
	friend class Setter;
	friend class In<T>;
	friend class IO<T>;
protected:
	std::set<In<T> *> out;
	T v;
	void update(In<T> *exclude = 0);
public:
	Var(T v_) : v(v_) {}
	Var() {}
	const T get();
	void connect(In<T> *f, bool notify);
};

template <class T> class VarE : public Var<T> {
	friend class Setter;
	friend class Out<T>;
	Out<T> *in;
public:
	VarE(T v) : Var<T>(v), in(0) {}
	VarE() : Var<T>(), in(0) {}
	void freeze() { delete in; }
	void connect(Out<T> *f);
};

template <class T> class VarI : public Var<T> {
public:
	VarI(T v) : Var<T>(v) {}
	VarI() : Var<T>() {}
};

template <class T> class In {
	friend class Setter;
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
	friend class Setter;
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

/* duplicate base type ‘IO<int>’ invalid :(
template <class X, class Y> class BiFun : public IO<X>, IO<Y> {
public:
	virtual Y run1(X &x) = 0;
	virtual X run2(Y &y) = 0;
	virtual void notify(X &x) { IO<Y>::set(run1(x)); }
	virtual void notify(Y &y) { IO<X>::set(run1(y)); }
};
*/
template <class X, class Y> class BiFun {
	friend class Setter;
	class Part1 : public IO<X> {
		friend class BiFun;
		BiFun<X,Y> *p;
		Part1(BiFun *p_) : p(p_) {}
	public:
		virtual ~Part1() { p->part1 = 0; delete p; }
		virtual void notify(X &x) { p->part2->set(p->run1(x)); }
	};
	Part1 *part1;
	class Part2 : public IO<Y> {
		friend class BiFun;
		BiFun<X,Y> *p;
		Part2(BiFun *p_) : p(p_) {}
	public:
		virtual ~Part2() { p->part2 = 0; delete p; }
		virtual void notify(Y &y) { p->part1->set(p->run2(y)); }
	};
	Part2 *part2;
public:
	virtual Y run1(X &x) = 0;
	virtual X run2(Y &y) = 0;
	BiFun() {
		part1 = new Part1(this);
		part2 = new Part2(this);
	}
	~BiFun() {
		if (part1) delete part1;
		if (part2) delete part2;
	}
};

class Setter : public Atomic {
public:
	template <class T> void set(VarE<T> &, T);
	template <class T> void set(VarI<T> &, T);
	template <class T> void set(VarE<T> &y, Var<T> &x);
	template <class X, class Y> void set(VarE<Y> &y, Fun<X, Y> *f, Var<X> &x);
	template <class T> void identify(VarI<T> &y, VarI<T> &x);
	template <class X, class Y> void identify(VarI<Y> &y, BiFun<X, Y> *f, VarI<X> &x);

	template <class T> const T &ref(Var<T> &v) { return v.v; }
	// write_refs are evil
	template <class T> T &write_ref(Var<T> &v) { return v.v; }
};

template <class T> void Var<T>::connect(In<T> *f, bool notify) {
	Atomic a;
	out.insert(f);
	f->parent = this;
	if (notify)
		f->notify(v);
}

template <class T> void VarE<T>::connect(Out<T> *f) {
	Atomic a;
	in = f;
	f->parent = this;
}

template <class T> void Var<T>::update(In<T> *exclude) {
	for (typename std::set<In<T> *>::iterator i = out.begin(); i != out.end(); i++) {
		In<T> *cur = *i;
		if (cur != exclude)
			cur->notify(v);
	}
}

template <class T> const T Var<T>::get() {
	Atomic a;
	return v;
}

template <class X> class Identity : public Fun<X,X> {
	X run(X &x) { return x; }
};

template <class X, class Y> void Setter::set(VarE<Y> &y, Fun<X, Y> *f, Var<X> &x) {
	y.freeze();
	y.connect(f);
	x.connect(f, true);
}

template <class T> void Setter::set(VarE<T> &y, Var<T> &x) {
	set(y, new Identity<T>, x);
}

template <class T> void Setter::set(VarE<T> &y, T x) {
	y.freeze();
	y.v = x;
	y.update();
}

template <class T> void Setter::set(VarI<T> &y, T x) {
	y.v = x;
	y.update();
}

template <class X> class BiIdentity : public BiFun<X,X> {
	X run1(X &x) { return x; }
	X run2(X &x) { return x; }
};

template <class X, class Y> void Setter::identify(VarI<Y> &y, BiFun<X, Y> *f, VarI<X> &x) {
	y.connect(f->part2, false);
	x.connect(f->part1, true);
}

template <class T> void Setter::identify(VarI<T> &y, VarI<T> &x) {
	identify(y, new BiIdentity<T>, x);
}
#endif
