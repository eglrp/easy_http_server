#include "thread_pool.h"
#include "mutex.h"

#include <unistd.h>
#include <stdio.h>
#include <signal.h>

thread_pool tp;
mutex mtx;
volatile int counter = 0;
volatile bool stop = false;

void add(void* arg)
{
	while(!stop) {
		lock_guard lock(mtx);
		++counter;
		printf("counter now is %d\n", counter);
		sleep(1);
	}
}

void handler(int sig)
{
	stop = true;
}

int main()
{
	signal(SIGINT, handler);
	tp.set_pool_size(4);
	tp.start();
	task* tk1 = new task(add, NULL);
	task* tk2 = new task(add, NULL);
	task* tk3 = new task(add, NULL);
	task* tk4 = new task(add, NULL);
	tp.add_task(tk1);
	tp.add_task(tk2);
	tp.add_task(tk3);
	tp.add_task(tk4);
	printf("over\n");
	stop = true;
	return 0;
}
