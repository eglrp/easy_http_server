#ifndef _HTTP_CONTEXT_H
#define _HTTP_CONTEXT_H

#include "http_request.h"
#include "http_response.h"

class http_context {
public:
	http_context(int fd);
	~http_context();
public:
	int  record_start_time();  
	int  get_cost_time();
	void clear();
	void delete_request_and_response();
public:
	response& get_response();
	request&  get_request();
private:
	int       fd_;
	timeval   start_;
	request*  req_;
	response* res_;
};

#endif
