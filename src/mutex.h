#ifndef _MUTEX_H
#define _MUTEX_H

#include "utili.h"

#include <pthread.h>
#include <assert.h>

class mutex {
public:
	mutex() : is_locked_(false) {
		pthread_mutex_init(&locker_, NULL);
	}
	~mutex() {
		while(is_locked_); 
		assert(!is_locked_);
		int ret = pthread_mutex_destroy(&locker_);
		CHECK(ret, "mutex destroy err: %d", ret);
	}
public:
	void lock() {
		pthread_mutex_lock(&locker_);
		is_locked_ = true;
	}
	void unlock() {
		is_locked_ = false;   //do it before unlocking to avoid race condition
		int ret = pthread_mutex_unlock(&locker_);
		CHECK(ret, "mutex unlock err: %d", ret);
	}
	pthread_mutex_t* get_mutex() {
		return &locker_;
	}
private:
	pthread_mutex_t locker_;
	volatile bool   is_locked_;
};

class lock_guard {
public:
	explicit lock_guard(mutex& mtx) : mtx_(mtx) {
		mtx_.lock();
	}
	~lock_guard() {
		mtx_.unlock();
	}
private:
	mutex& mtx_;
};

#endif
