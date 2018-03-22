#include <iostream>
#include <sstream>
#include <assert.h>
#include <string.h>
using namespace std;

int readsome(stringstream& ss, char* buffer, int read_once, int& read_size)
{
	ss.read(buffer, read_once);
	read_size = ss.gcount();

	if(!ss.eof())
		return 1;
	return 0;
}

int rollback(stringstream& ss, int num)
{
	if(ss.eof())
		ss.clear();
	
	int rb_pos = (int)ss.tellg() - num;
	ss.seekg(rb_pos);
	return ss.good() ? 0 : -1;
}

int main()
{
	const char *s1 = "hello";
	int v1 = 10;
	const char *s2 = "world";
	stringstream ss;
	ss<<s1<<v1<<s2;
	
	cout<<ss.str().c_str()<<endl;

	int read_count = 0;
	int read_once = 5;
	char buffer[20];
	int ret = readsome(ss, buffer, read_once, read_count);
	cout<<"first buffer: "<<buffer<<" read_count: "<<read_count<<endl;
	assert(ret == 1);


	read_once <<= 1;
	ret = readsome(ss, buffer, read_once, read_count);
	if(ret == 0) {
		cout<<"read over"<<endl;
	}
	//bufer clear automatic
	cout<<"second buffer: "<<buffer<<" read_count: "<<read_count<<endl;
	cout<<"current sstream.str is: "<<ss.str().c_str()<<endl;

	//rollback
	int back = read_count;

	rollback(ss, read_count);

	cout<<"when rollback, the final str is: "<<ss.str().c_str()<<endl;

	//don't need to invoke memset, it's work automatically
	readsome(ss, buffer, read_once, read_count);
	cout<<"hehe, shi guang dao liu, wo you neng du yi hui: "<<buffer<<endl;

	//need mesert, because stringstream is empty, it doesn't work right now!
	memset(buffer, 0, sizeof(buffer));  //
	readsome(ss, buffer, read_once, read_count);
	cout<<"Now i can'r read more: "<<buffer<<endl;

	return 0;
}
