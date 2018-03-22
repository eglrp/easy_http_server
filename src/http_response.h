#ifndef _HTTP_RESPONSE_H
#define _HTTP_RESPONSE_H

#include <json/json.h>

#include <string>
#include <sstream>
#include <map>
#include <sys/time.h>

struct code_message {
	int status_code_;
	std::string msg_;
};

static const code_message STATUS_OK = {200, "OK"};
static const code_message STATUS_NOT_FOUND = {404, "Not Found"};
static const code_message STATUS_METHOD_NOT_ALLOWED = {405, "Method Not Allowed"};
static const code_message STATUS_LENGTH_REQUIRED = {411, "Length Required"};

class response {
public:
	response(code_message stats_code = STATUS_OK);
	response(code_message status_code, Json::Value& body);
public:
	void set_head(std::string name, std::string value);
	void set_body(Json::Value& body);
	void set_body(std::string body);
	int  gen_response(std::string& http_version, bool is_keepalive);
	int  readsome(char* buffer, int buffer_size, int& read_size);    //return 0: read part, 1: read over, -1: read error
	int  rollback(int num);
public:
	bool 		 is_writed_;
	code_message code_msg_;
	std::string  body_;
private:
	std::map<std::string, std::string> headers_;
	std::stringstream                  res_bytes_;
};

#endif
