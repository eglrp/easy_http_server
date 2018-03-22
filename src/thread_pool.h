#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include "mutex.h"
#include "condition.h"

#include <vector>
#include <deque>
#include <pthread.h>

const int STARTED = 0;
const int STOPPED = 1;

class task {
	typedef void (*task_callback)(void *);
public:
	task(task_callback task_cb, void* arg) : task_cb_(task_cb), arg_(arg) 
	{}
	//default dtor is okay
public:
	void run() { task_cb_(arg_); }
private:
	task_callback task_cb_;
	void*         arg_;
};

class thread_pool {
	typedef void (*thread_start_callback)();
public:
	thread_pool();
	~thread_pool();
public:
	int   start();
	int   destroy();
	void* execute_thread();
	int   add_task(task* task);
	void  set_thread_start_cb(thread_start_callback f);
	void  set_task_size_limit(int size);
	void  set_pool_size(int pool_size);
public:
	thread_start_callback  cb_;
private:
	mutex                  task_mutex_;
	condition              task_cond_;
	std::vector<pthread_t> threads_;
	std::deque<task *>     task_queue_;
	int                    task_size_limit_;
	int         		   pool_size_;
	volatile int           pool_state_;
};

inline void thread_pool::set_thread_start_cb(thread_start_callback f)
{
	cb_ = f;
}

inline void thread_pool::set_task_size_limit(int size)
{
	task_size_limit_ = size;
}

inline void thread_pool::set_pool_size(int pool_size)
{
	pool_size_ = pool_size;
}

#endif
