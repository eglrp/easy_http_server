#include <iostream>

#define dbgprintf(format, args...) \
	fprintf(stderr, format, ##args)

int main()
{
	dbgprintf("%s", "hello");
	return 0;
}
