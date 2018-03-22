#include <stdio.h>
#include <thread>
#include <unistd.h>

#include "mutex.h"

volatile int counter = 0;
mutex mtx;

void worker1()
{
	while(1) {
		sleep(1);
		lock_guard lock(mtx);
		sleep(1);
		printf("worker1 do when counter = %d\n", counter);
		++counter;
		printf("worker1 do end counter = %d\n", counter);
	}
}

void worker2()
{
	while(1) {
		lock_guard lock(mtx);
		sleep(1);
		printf("worker2 do when counter = %d\n", counter);
		++counter;
		printf("worker2 do end counter = %d\n", counter);	
	}
}

int main()
{
	std::thread t1(worker1);
	std::thread t2(worker2);
	t1.join();
	t2.join();
	return 0;
}
