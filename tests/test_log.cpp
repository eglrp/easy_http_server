#include "log.h"
#include <stdio.h>

int main()
{
    int ret = log_init("../conf", "log.conf");
    if (ret != 0) {
        printf("log init error!");
        return 0;
    }		
	LOG_DEBUG("%s", "hello");
	LOG_WARN("%s", "world");
	LOG_INFO("%s", "hello");
	LOG_ERROR("%s", "coder");
}
