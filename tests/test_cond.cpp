#include "mutex.h"
#include "condition.h"

#include <thread>

mutex mt;
condition cond(mt);
int cnt=0;

void worker1()
{
	lock_guard lock(mt);
	printf("woker1 tag 1\n");
	while(cnt != 3){
		printf("worker wait and unlock\n");
		cond.wait();
	}
	printf("worker1 wake up and unlock\n");
	printf("worker get, the cnt is: %d\n", cnt);
	printf("worker1 died\n");
}

void worker2()
{
	lock_guard lock(mt);
	for(int i=0; i<4; ++i){
		printf("i=%d\n", i);
		cnt = i;
	}
	cond.signal();
	printf("worker2 died\n");
}

int main()
{
	std::thread t1(worker1);
	std::thread t2(worker2);
	t1.join();
	t2.join();
	printf("main died\n");
	return 0;
}
