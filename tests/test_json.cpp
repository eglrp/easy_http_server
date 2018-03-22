#include <iostream>
#include <json/json.h>
using namespace std;

int main()
{
	const char* str = "{\"user_id\": \"06141007\", \"passwd\": \"3a5cd\", \"user_name\": \"Tom\", \"degree\": 47}";

	Json::Reader reader;
	Json::Value root;
	if(reader.parse(str, root)) {
		std::string user_id = root["user_id"].asString();
		std::string passwd = root["passwd"].asString();
		std::string user_name = root["user_name"].asString();
		int degree = root["degree"].asInt();

		cout<<user_id<<endl;
		cout<<passwd<<endl;
		cout<<user_name<<endl;
		cout<<degree<<endl;
	}
	Json::FastWriter writer;
	string output = writer.write(root);
	cout<<output;
	return 0;
}
