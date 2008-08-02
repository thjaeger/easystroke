#include "var.h"

Glib::StaticRecMutex global_mutex = GLIBMM_STATIC_REC_MUTEX_INIT;

class BiIntLong : public BiFun<int,long> {
	long run1(const int &x) { return x; }
	int run2(const long &x) { return x; }
};

class LongInt : public Fun<long,int> {
	int run(const long &x) { return x; }
};

void test() {
	VarE<int> x(0);
	VarE<int> y(1);
	VarE<long> z(1);
	y.assign(x);
	x.set(2);
	printf("2 == %d\n", y.get());
	z.set(3L);
	y.assign(new LongInt, z);
	printf("3 == %d\n", y.get());
	VarI<int> a(0);
	VarI<long> b(1);
	VarI<int> c(2);
	b.identify(new BiIntLong, a);
	a.identify(c);
	printf("2 == %d == %ld == %d\n", a.get(), b.get(), c.get());
	c.set(3);
	printf("3 == %d == %ld == %d\n", a.get(), b.get(), c.get());
}

#if TEST_VAR
int main(int, char**) {
	test();
	return 0;
}
#endif
