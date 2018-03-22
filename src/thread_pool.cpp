#include "thread_pool.h"
#include "log.h"

thread_pool::thread_pool() 
	: cb_(NULL), task_mutex_(), task_cond_(task_mutex_), 
	  task_size_limit_(-1), pool_size_(0), pool_state_(-1)
{
}

thread_pool::~thread_pool()
{
	if(pool_state_ != STOPPED) //release resources
		destroy();
}

static void* start_thread(void* arg)   //per thread start working function
{
	thread_pool *tp = static_cast<thread_pool*>(arg);
	if(tp->cb_ != NULL) {
		tp->cb_();
	}
	else {
		LOG_DEBUG("thread start cb is null");
	}

	tp->execute_thread();
	return static_cast<void*>(0);
}

int thread_pool::start()
{
	if(pool_size_ == 0) {
		LOG_ERROR("pool size must be set!");
		return -1;
	}

	if(pool_state_ == STARTED) {
		LOG_WARN("thread_pool has been started!");
	}

	pool_state_ = STARTED;
	
	int ret = 0;
	for(int i=0; i<pool_size_; ++i){
		pthread_t tid;
		ret = pthread_create(&tid, NULL, start_thread, static_cast<void*>(this));
		if(ret != 0) {
			LOG_ERROR("pthread create failed: %d", ret);
			return -1;
		}
		threads_.push_back(tid);
	}
	LOG_DEBUG("%d threads created by the thread pool", pool_size_);

	return 0;
}

void* thread_pool::execute_thread()
{
	task* tk;
	LOG_DEBUG("starting thread: %u", pthread_self());
	
	while(true) {
		//try to acquire a task
		task_mutex_.lock();
		LOG_DEBUG("locking: %u", pthread_self());
		
		//only two conditon to relieve wait
		while((pool_state_ != STOPPED) && (task_queue_.empty())) {
			LOG_DEBUG("unlocking and waiting: %u", pthread_self());
			task_cond_.wait();
			LOG_DEBUG("signaled and locking: %u", pthread_self());
		}

		if(pool_state_ == STOPPED) {
			LOG_INFO("unlocking and exiting: %u", pthread_self());
			task_mutex_.unlock();
			pthread_exit(NULL);    // exit right now !!!
		}
		
		tk = task_queue_.front();
		task_queue_.pop_front();
		LOG_DEBUG("unlocking: %u", pthread_self());
		task_mutex_.unlock();   // out the critical region

		tk->run();
		delete tk;
	}
	return static_cast<void*>(0);
}

int thread_pool::add_task(task* tsk)
{
	task_mutex_.lock();

	if((task_size_limit_) > 0 && (static_cast<int>(task_queue_.size()) > task_size_limit_)) {
		LOG_WARN("task size reach limit: %d", task_size_limit_);
		task_mutex_.unlock();
	}
	task_queue_.push_back(tsk);

	//wake up one thread that is waiting for a task to be available
	task_cond_.signal();

	task_mutex_.unlock();

	return 0;
}

int thread_pool::destroy()
{
	//note: this is not for synchronization, its for thread communication !
	//destroy() will only be called from the main thread, yet the modified pool_state_
	//may not show up to other threads until its modified in a lock !
	{
		lock_guard lock(task_mutex_);
		pool_state_ = STOPPED;
	}
	LOG_INFO("broadcasting STOP signal to all threads...");
	task_cond_.broadcast();  //notify all threads we are shutting down

	int ret = -1;
	for(int i=0; i<pool_size_; ++i) {
		ret = pthread_join(threads_[i], NULL);
		LOG_DEBUG("pthread_join() returned %d", ret);
		task_cond_.broadcast();   //try waking up a bunch of threads that are still waiting
	}

	LOG_INFO("%d threads exited from the thread pool, task size: %u", pool_size_, task_queue_.size());
	return 0;
}
