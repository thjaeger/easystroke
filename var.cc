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

#if TEST_VAR
int plus(int x, int y) { return x + y; }

void test() {
	Source<int> x(0);
	Var<int> y(x);
	Fun<int, long> *z = fun(&convert<int, long>, x);
	Var<long> w(*z);
	Fun2<int, int, int> *v = fun2(&plus, x, y);

	printf("0 == %d == %d == %ld == %ld\n", x.get(), y.get(), z->get(), w.get());
	printf("%d = %d + %d\n", v->get(), x.get(), y.get());
	x.set(2);
	printf("2 == %d == %d == %ld == %ld\n", x.get(), y.get(), z->get(), w.get());
	printf("%d = %d + %d\n", v->get(), x.get(), y.get());
}

int main(int, char**) {
	test();
	return 0;
}
#endif
