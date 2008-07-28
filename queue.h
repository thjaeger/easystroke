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
