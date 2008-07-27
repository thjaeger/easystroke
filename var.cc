#include "var.h"

Glib::StaticRecMutex Setter::mutex = GLIBMM_STATIC_REC_MUTEX_INIT;

template <class T> void Var<T>::update(In<T> *exclude) {
	for (typename std::set<In<T> *>::iterator i = out.begin(); i != out.end(); i++) {
		In<T> *cur = *i;
		if (cur != exclude)
			cur->notify(v);
	}
}

template <class T> const T Var<T>::get() { 
	Glib::RecMutex::Lock foo(Setter::mutex); 
	return v; 
}

template <class X> class Identity : public Fun<X,X> {
	X run(X &x) { return x; }
};

template <class X, class Y> void Setter::set(VarE<Y> &y, Fun<X, Y> *f, Var<X> &x) {
	y.freeze();
	y.v = f->run(x.v);
	y.in = f;
	x.out.insert(f);
	((In<X> *)f)->parent = &x;
	((Out<Y> *)f)->parent = &y;
	y.update();
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
	y.v = f->run1(x.v);
	x.out.insert(f->part1);
	y.out.insert(f->part2);
	f->part1->parent = &x;
	f->part2->parent = &y;
	y.update(f->part2);
}

template <class T> void Setter::identify(VarI<T> &y, VarI<T> &x) {
	identify(y, new BiIdentity<T>, x);
}

class BiIntLong : public BiFun<int,long> {
	long run1(int &x) { return x; }
	int run2(long &x) { return x; }
};

void test() {
	VarE<int> x(0);
	VarE<int> y(1);
	VarE<long> z(1);
	Setter s;
	s.set(y,x);
	s.set(x,2);
	printf("2 == %d\n", y.get());
	s.set(z,3L);
	FUN(int, f, long, x, x);
	s.set(y, f, z);
	printf("3 == %d\n", y.get());
	VarI<int> a(0);
	VarI<long> b(1);
	VarI<int> c(2);
	s.identify(b, new BiIntLong, a);
	s.identify(a, c);
	printf("2 == %d == %ld == %d\n", a.get(), b.get(), c.get());
	s.set(c, 3);
	printf("3 == %d == %ld == %d\n", a.get(), b.get(), c.get());
}
