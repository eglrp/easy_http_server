#include "http_request.h"
#include "log.h"

static const int MAX_REQ_SIZE = 10485760;

/////////////////////////////////////////////////////////////////////////////////////
//request_param

int request_param::parse_query_url(std::string& query_url)
{
	std::stringstream query_ss(query_url);
	LOG_DEBUG("start parse_query_url: %s", query_url.c_str());

	while(query_ss.good()) {
		std::string key_value;
		std::getline(query_ss, key_value, '&');
		LOG_DEBUG("get key_value: %s", key_value.c_str());

		std::stringstream key_value_ss(key_value);
		while(key_value_ss.good()) {
			std::string key, value;
			std::getline(key_value_ss, key, '=');
			std::getline(key_value_ss, value, '=');
			
			params_.insert(std::pair<std::string, std::string>(key, value));
		}
	}
	return 0;
}

std::string request_param::get_param(std::string& name)
{
	auto iter = params_.find(name);
	if(iter == params_.end()) {
		return std::string();
	}
	return iter->second;
}

void request_param::get_params(std::string& name, std::vector<std::string>& output_params)
{
	auto range = params_.equal_range(name);  //the key "name" maybe has more than one value, 
									         //equal_range is a method to traverse all values like this for multimap
	for(auto it = range.first; it != range.second; ++it) {
		output_params.push_back(it->second);
	}
}

///////////////////////////////////////////////////////////////////
//request_line

void request_line::append_request_url(std::string url)
{
	request_url_ = url;
}

std::string request_line::get_request_uri()
{
	std::stringstream ss(get_request_url());
	std::string uri;
	std::getline(ss, uri, '?');
	return uri;
}

std::string request_line::get_request_url()
{
	return request_url_;
}

void request_line::set_method(std::string m)
{
	method_ = m;
}

std::string request_line::get_method()
{
	return method_;
}

void request_line::set_http_version(std::string v) 
{
	http_version_ = v;
}

std::string request_line::get_http_version()
{
	return http_version_;
}

request_param& request_line::get_request_param()
{
	return line_param_;
}

int request_line::parse_request_url_params() // request_ine param !
{
	std::stringstream ss(request_url_);
	LOG_DEBUG("start parse params which request_url: %s", request_url_.c_str());

	std::string uri;
	std::getline(ss, uri, '?');
	if(ss.good()) {
		std::string query_param;
		std::getline(ss, query_param, '?');

		line_param_.parse_query_url(query_param);
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////
//request_body

std::string* request_body::get_raw_string()
{
	return &raw_string_;
}

request_param* request_body::get_req_params()
{
	return &body_params_;
}

std::string request_body::get_param(std::string name)
{
	return body_params_.get_param(name);
}

//maybe the body params is not only one, and server want wo know all body params
void request_body::get_params(std::string& name, std::vector<std::string>& params)
{
	return body_params_.get_params(name, params);
}

/////////////////////////////////////////////////////////////////////
//http-parser running functions

int on_url(http_parser* p, const char* buf, size_t len)
{
	request *req = static_cast<request *>(p->data);
	std::string url;
	url.assign(buf, len);
	req->line_.append_request_url(url);
	return 0;
}

int on_header_field(http_parser *p, const char* buf, size_t len)
{
	request *req = static_cast<request *>(p->data);
	if(req->parse_part_ == PARSE_REQ_LINE) {
		//////
		if(p->method == 1) {  //p is http-parser
			req->line_.set_method("GET");
		}
		if(p->method == 3) {
			req->line_.set_method("POST");
		}

		int ret = req->line_.parse_request_url_params();
		if(ret != 0) 
			return ret;

		if(p->http_major == 1 && p->http_minor == 0) {
			req->line_.set_http_version("HTTP/1.0");
		}
		else if(p->http_major == 1 && p->http_minor == 1) {
			req->line_.set_http_version("HTTP/1.1");
		}
		req->parse_part_ = PARSE_REQ_HEAD;
	}

	std::string field;
	field.assign(buf, len);
	if(req->last_was_value_) {
		req->header_fields_.push_back(field);
		req->last_was_value_ = false;
	}
	else {
		req->header_fields_[req->header_fields_.size()-1] += field;
	}

	LOG_DEBUG("GET field: %s", field.c_str());
	return 0;
}

int on_header_value(http_parser *p, const char* buf, size_t len)
{
	request *req = static_cast<request *>(p->data);

	std::string value;
	value.assign(buf, len);
	if(!req->last_was_value_) {
		req->header_values_.push_back(value);
	}
	else {
		req->header_values_[req->header_values_.size()-1] += value;
	}

	LOG_DEBUG("GET value: %s", value.c_str());
	req->last_was_value_ = true;
	return 0;
}

int on_headers_complete(http_parser *p)
{
	request *req = static_cast<request *>(p->data);
	
	if(req->header_fields_.size() != req->header_values_.size()) { //check size
		LOG_ERROR("header field size: %u != value size: %u", req->header_fields_.size(), req->header_values_.size());
		return -1;
	}

	for(size_t i=0; i<req->header_fields_.size(); ++i) {
		req->add_header(req->header_fields_[i], req->header_values_[i]);   //add all to map
	}
	
	req->parse_part_ = PARSE_REQ_HEAD_OVER;      //head over
	
	LOG_DEBUG("HERDERS COMPLETE! whick field size: %u, valuse size: %u", 
			  req->header_fields_.size(), req->header_values_.size());
	
	//!!!　　POST need length,     
	//FIXME            FIXME            FIXME            FIXME              FIXME
	if(req->get_method() == "POST" && req->get_header("Content-Length").empty()) {
		req->parse_err_ = PARSE_LEN_REQUIRED;   //FIXME
		return -1;
	}
	return 0;
}

int on_body(http_parser *p, const char* buf, size_t len)
{
	request *req = static_cast<request *>(p->data);
	req->get_body()->get_raw_string()->append(buf, len);  //add all body to request_body.raw_string member
	req->parse_part_ = PARSE_REQ_BODY;
	LOG_DEBUG("GET body len: %d", len);
	return 0;
}

int on_message_complete(http_parser *p)
{
	request *req = static_cast<request *>(p->data);

	//parse body params //POST
	if(req->get_header("Content-Type") == "application/x-www-form-urlencoded") {
		std::string *raw_str = req->get_body()->get_raw_string();  //if size != 0, we will parse params
		if(raw_str->size()) {
			//once wo did this, the request_body.req_params will be fill
			req->get_body()->get_req_params()->parse_query_url(*raw_str);  //maybe body has param
		}
	}
	
	req->parse_part_ = PARSE_REQ_OVER;
	LOG_DEBUG("msg COMPLETE");
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////
//request
request::request()
	: last_was_value_(true), //add new field for first
	  parse_part_(PARSE_REQ_LINE), 
	  parse_err_(0), 
	  total_req_size_(0)
{
	http_parser_settings_init(&settings_);
	settings_.on_url = on_url;
	settings_.on_header_field = on_header_field;
	settings_.on_header_value = on_header_value;
	settings_.on_headers_complete = on_headers_complete;
	settings_.on_body = on_body;
	settings_.on_message_complete = on_message_complete;

	http_parser_init(&parser_, HTTP_REQUEST);
	parser_.data = this;
}


// this function will be called in main
int request::parse_request(const char* read_buffer, int read_size)
{
	total_req_size_ += read_size;
	if(total_req_size_ > MAX_REQ_SIZE) {  //MAX-LEN
		LOG_INFO("TOO BIG REQUEST WE WILL REFUSE IT!");
		return -1;
	}

	LOG_DEBUG("read from client: size: %d, content: %s", read_size, read_buffer);
	
	ssize_t nparsed = http_parser_execute(&parser_, &settings_, read_buffer, read_size);
	if(nparsed != read_size) {
		std::string err_msg = "unknow";
		if(parser_.http_errno) {
			err_msg = http_errno_description(HTTP_PARSER_ERRNO(&parser_));
		}
		LOG_ERROR("parse request error! msg: %s", err_msg.c_str());
		return -1;
	}

	if(parse_err_) // PARSE_LEN_REQUIRED
		return parse_err_;

	if(parse_part_ != PARSE_REQ_OVER)
		return NEED_MORE_STATUS;

	return 0;
}

//add all header to map
void request::add_header(std::string& name, std::string& value)
{
	headers_[name] = value;
}

std::string request::get_header(std::string name)
{
	return headers_[name];
}

std::string request::get_method() 
{
	return line_.get_method();
}

request_body* request::get_body()
{
	return &body_;
}

std::string request::get_request_uri()  //for http_server check if support this uri
{
	return line_.get_request_uri();
}

std::string request::get_param(std::string name)
{
	if(line_.get_method() == "GET") {
		return line_.get_request_param().get_param(name);  //GET -> line
	} 
	else if(line_.get_method() == "POST") {   //POST -> body
		return body_.get_param(name);
	}
	return std::string();
}

////////////////////////////////////////////

#define IS_HEX(ch)   ((ch >= 'a' && ch <= 'f') \
				   || (ch >= 'A' && ch <= 'F') \
				   || (ch >= '0' && ch <= '9'))
//ASCII
int unescape(std::string &param, std::string& unescape_param)
{
	const int size = param.size();
	for(int i=0; i<size; ++i) {
		if(('%' == param[i]) && IS_HEX(param[i+1]) && IS_HEX(param[i+2])) {
			std::string tmp;
			tmp += param[i+1];
			tmp += param[i+2];
			char *ptr;
			unescape_param.append(1, (unsigned char)strtol(tmp.c_str(), &ptr, 16));
			i += 2;
		}
		else if(param[i] == '+') {
			unescape_param.append(" ");
		}
		else {
			unescape_param.append(1, param[i]);
		}
	}
	return 0;
}

/////////////////////////////////////

std::string request::get_unescape_param(std::string name)
{
	std::string param = get_param(name);
	if(param.empty()) {
		return param;
	}

	std::string unescape_param;
	unescape(param, unescape_param);
	return unescape_param;
}

