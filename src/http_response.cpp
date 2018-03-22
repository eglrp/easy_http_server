#include "http_response.h"
#include "log.h"

#include <sstream>
#include <stdlib.h>
#include <algorithm>

static const char* const EASY_HTTP_VERSION = "0.9.3";

response::response(code_message status_code)
	: is_writed_(0), code_msg_(status_code)
{
}

void response::set_head(std::string name, std::string value)
{
	headers_[name] = value;
}

void response::set_body(Json::Value& body)
{
	Json::FastWriter writer;
	std::string str_value = writer.write(body);
	body_ = str_value;
}

void response::set_body(std::string body)
{
	body_ = body;
}

int response::gen_response(std::string& http_version, bool is_keepalive)
{
	LOG_DEBUG("START gen_response code: %d, msg: %s", code_msg_.status_code_, code_msg_.msg_.c_str());
	//res_bytes is a stringstream type
	res_bytes_<<http_version<<" "<<code_msg_.status_code_<<" "<<code_msg_.msg_<<"\r\n";
	res_bytes_<<"Server: easy_http/"<<EASY_HTTP_VERSION<<"\r\n";

	//if not have Content-Type
	if(headers_.find("Content-Type") == headers_.end()) {
		res_bytes_<<"Content-Type: application/json; charset=UTF-8"<<"\r\n";
	}

	//if not have Content-Length
	if(headers_.find("Content-Length") == headers_.end()) {
		res_bytes_<<"Content-Length: "<<body_.size()<<"\r\n";
	}

	std::string con_status = "Connection: close";
	if(is_keepalive) {
		con_status = "Connection: Keep-Alive";
	}
	res_bytes_<<con_status<<"\r\n";

	for(auto i : headers_) {
		res_bytes_<<i.first<<": "<<i.second<<"\r\n";
	}

	//header end
	res_bytes_<<"\r\n";
	res_bytes_<<body_;

	LOG_DEBUG("gen response context: %s", res_bytes_.str().c_str());
	return 0;
}

//buffer_size is WRITE_BUFFER_SIZE
int response::readsome(char* output_buffer, int buffer_size, int& read_size)
{
	res_bytes_.read(output_buffer, buffer_size);
	read_size = res_bytes_.gcount();

	if(!res_bytes_.eof()) {
		return 1;
	}
	return 0;
}

int response::rollback(int num)
{
	if(res_bytes_.eof()) {
		res_bytes_.clear();
	}
	
	int rollback_pos = (int)res_bytes_.tellg() - num;
	res_bytes_.seekg(rollback_pos);
	return res_bytes_.good() ? 0 : -1;
}

