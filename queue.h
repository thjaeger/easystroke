#ifndef __QUEUE_H__
#define __QUEUE_H__
#include <glibmm/thread.h>
#include <glibmm/dispatcher.h>
#include <queue>

template<class T> class Queue {
	std::queue<T> q;
	Glib::Mutex m;
	Glib::Dispatcher d;
	sigc::slot<void,T &> cb;
	void on_insert() {
		T x;
		{
			Glib::Mutex::Lock l(m);
			x = q.front();
			q.pop();
		}
		cb(x);
	}
	Queue &operator=(Queue &) { printf("Queue: Assignment unsupported\n"); }
	Queue(Queue &) { printf("Queue: Copying unsupported\n"); }
public:
	Queue(sigc::slot<void, T &> cb_) : cb(cb_) {
		d.connect(sigc::mem_fun(*this, &Queue<T>::on_insert));
	}
	void push(T &x) {
		{
			Glib::Mutex::Lock l(m);
			q.push(x);
		}
		d();
	}
};

#endif
