#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include "http_parser.h"

#include "json/json.h"

#include <string>
#include <sstream>
#include <map>
#include <vector>

static const int PARSE_REQ_LINE = 0;
static const int PARSE_REQ_HEAD = 1;
static const int PARSE_REQ_BODY = 2;
static const int PARSE_REQ_OVER = 3;
static const int PARSE_REQ_HEAD_OVER = 4;

static const int NEED_MORE_STATUS = 1;
static const int PARSE_LEN_REQUIRED = 2;  //need length


class request_param {
public:
	std::string get_param(std::string& name);
	void get_params(std::string& name, std::vector<std::string>& params);
	int  parse_query_url(std::string& query_url);  //query_url : name=tom&age=3
private:
	std::multimap<std::string, std::string> params_;
};

class request_line {
public:
	std::string    get_request_uri();
	std::string    get_request_url();
	
	void           set_request_url(std::string url);
	void           append_request_url(std::string url);
	
	request_param& get_request_param();
	
	int            parse_request_url_params();

	std::string    get_method();
	void           set_method(std::string m);


	std::string    get_http_version();
	void           set_http_version(std::string v);
private:
	request_param line_param_;
	std::string   method_;   	 //GET/POST
	std::string   request_url_;  // /hello?name=aaa
	std::string   http_version_; // HTTP/1.1
};

class request_body {
public:
	std::string    get_param(std::string name);   //key:name value?  only get the first one
	void           get_params(std::string& name, std::vector<std::string>& output_params);  //key:name value:many  get all values 
	std::string*   get_raw_string();
	request_param* get_req_params();
private:
	std::string   raw_string_;
	request_param body_params_;
};

class request {
public:
	request();
	//default dtor is okay
public:
	std::string   get_param(std::string name);
	std::string   get_unescape_param(std::string name);
	void          get_params(std::string& name, std::vector<std::string>& params);
	void          add_header(std::string& name, std::string& value);
	std::string   get_method();
	std::string   get_header(std::string name);
	std::string   get_request_uri();
	int           parse_request(const char* read_buffer, int read_size);
	int           clear();
	request_body* get_body();
public:
	bool          			 last_was_value_;
	std::vector<std::string> header_fields_;
	std::vector<std::string> header_values_;
	int                      parse_part_;
	int                      parse_err_;
	request_line             line_;
private:
	std::map<std::string, std::string> headers_;
	int                                total_req_size_;
	request_body                       body_;
	http_parser_settings               settings_;
	http_parser                        parser_;
};

#endif
