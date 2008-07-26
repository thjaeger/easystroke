#ifndef __LOCKING_H__
#define __LOCKING_H__

#include <glibmm/thread.h>
#include <boost/shared_ptr.hpp>

extern bool gui;

template <class T> class Ref;

template <class T> class Lock {
	friend class Ref<T>;

	boost::shared_ptr<T> x;
	boost::shared_ptr<Glib::Mutex> m;
public:
	Lock()           : x(new T)    { if (gui) m.reset(new Glib::Mutex); }
	Lock(const T &t) : x(new T(t)) { if (gui) m.reset(new Glib::Mutex); }
	virtual ~Lock() { }
	const T get() {
		if (!gui)
			return *x;
		Glib::Mutex::Lock l(*m);
		return *x;
	}
	void set(T x_) {
		if (!gui)
			*x = x_;
		Glib::Mutex::Lock l(*m);
		*x = x_;
	}
};


template <class T> class Ref {
	Glib::Mutex::Lock *lock;
	T &x;
public:
	Ref(Lock<T> &l) : lock(gui ? new Glib::Mutex::Lock(*l.m) : 0), x(*l.x) {}
	~Ref() { delete lock; }
	T *operator->() { return &x; }
	T &operator*() { return x; }
};

#endif
