#include "http_context.h"
#include "log.h"

http_context::http_context(int fd)
	: fd_(fd), req_(new request), res_(new response)
{
}

http_context::~http_context()
{
	delete_request_and_response();
}

int http_context::record_start_time()
{
	return gettimeofday(&start_, NULL);
}

request& http_context::get_request()
{
	return *req_;
}

response& http_context::get_response()
{
	return *res_;
}

int http_context::get_cost_time()
{
	timeval end;
	gettimeofday(&end, NULL);
	int cost_time = (end.tv_sec - start_.tv_sec) * 1000000 + (end.tv_usec - start_.tv_usec);
	return cost_time;
}

void http_context::clear()
{
	delete_request_and_response();   //http is no state protocol

	//renew
	req_ = new request();
	res_ = new response();
}

void http_context::delete_request_and_response()
{
	if(req_ != NULL) {
		delete req_;
		req_ = NULL;
	}
	if(res_ != NULL) {
		delete res_;
		res_ = NULL;
	}
}
