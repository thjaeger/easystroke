/*
 * Copyright (c) 2008, Thomas Jaeger <ThJaeger@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __QUEUE_H__
#define __QUEUE_H__
#include <glibmm/thread.h>
#include <glibmm/dispatcher.h>

template<class T> class Queue {
	T x;
	Glib::Mutex m;
	Glib::Dispatcher d;
	sigc::slot<void,T &> cb;
	void on_insert() {
		T x_ = x;
		m.unlock();
		cb(x_);
	}
	Queue &operator=(Queue &) { printf("Queue: Assignment unsupported\n"); }
	Queue(Queue &) { printf("Queue: Copying unsupported\n"); }
public:
	Queue(sigc::slot<void, T &> cb_) : cb(cb_) {
		d.connect(sigc::mem_fun(*this, &Queue<T>::on_insert));
	}
	void push(T &x_) {
		m.lock();
		x = x_;
		d();
	}
};

#endif
