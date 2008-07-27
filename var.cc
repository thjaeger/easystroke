#include "var.h"

Glib::StaticRecMutex Setter::mutex = GLIBMM_STATIC_REC_MUTEX_INIT;

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
