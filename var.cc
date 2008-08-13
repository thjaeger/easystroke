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

#include "var.h"
#include <map>

uint64 In::count = 0;

std::map<uint64, In *> dirty;

void mark_dirty(In *in) { dirty[in->index] = in; }

void update_dirty() {
	for (;;) {
		std::map<uint64, In *>::iterator i = dirty.begin();
		if (i == dirty.end())
			break;
		In *in = i->second;
		dirty.erase(i);
		in->notify();
	}
}

#if TEST_VAR
/*
class BiIntLong : public BiFun<int,long> {
	long run1(const int &x) { return x; }
	int run2(const long &x) { return x; }
};

*/
class IntLong : public Fun<int, long> {
	virtual long run(const int &x) { return x; }
public:
	IntLong(Out<int> &in) : Fun<int, long>(in) {}
};

void test() {
	Source<int> x(0);
	Var<int> y(x);
	IntLong z(y);
	Var<long> w(z);
	printf("0 == %d == %d == %ld == %ld\n", x.get(), y.get(), z.get(), w.get());
	x.set(2);
	printf("2 == %d == %d == %ld == %ld\n", x.get(), y.get(), z.get(), w.get());

	/*
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

	Collection<int> coll;
	Collection<int>::Ref r1 = coll.insert(new int(5));
	printf("1 == %d\n", r1->valid());
	coll.erase(r1);
	printf("0 == %d\n", r1->valid());
	*/
}

int main(int, char**) {
	test();
	return 0;
}
#endif
