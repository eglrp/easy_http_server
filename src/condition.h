#ifndef _CONDITION_H
#define _CONDITION_H

#include "mutex.h"

#include <pthread.h>

class condition {
public:
	condition(mutex& mtx) : mtx_(mtx) {
		pthread_cond_init(&cond_, NULL);
	}
	~condition() {
		pthread_cond_destroy(&cond_);
	}
public:
	void wait() {
		pthread_cond_wait(&cond_, mtx_.get_mutex());
	}
	void signal() {
		pthread_cond_signal(&cond_);
	}
	void broadcast() {
		pthread_cond_broadcast(&cond_);
	}
private:
	mutex&         mtx_;
	pthread_cond_t cond_;
};

#endif
