#include "http_server.h"
#include "http_context.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/epoll.h>

/////////////////////////////////////////////////////////////////////////
//http_method_decorator

http_method_decorator::http_method_decorator(int code, std::string name)
{
	codes_.insert(code);
	names_.insert(name);
}

http_method_decorator& http_method_decorator::operator|(http_method_decorator hm)
{
	std::set<int>* codes = hm.get_codes();
	codes_.insert(codes->begin(), codes->end());
	std::set<std::string>* names = hm.get_names();
	names_.insert(names->begin(), names->end());
	return *this;  //
}

std::set<std::string>* http_method_decorator::get_names()
{
	return &names_;
}

std::set<int>* http_method_decorator::get_codes() 
{
	return &codes_;
}

////////////////////////////////////////////////////////////////////////
//http_epoll_watcher

void http_epoll_watcher::add_mapping(std::string path, method_handler handler, http_method_decorator method)
{
	resource rsc = {method, handler, NULL};
	resource_map_[path] = rsc;
}

void http_epoll_watcher::add_mapping(std::string path, json_handler handler, http_method_decorator method)
{
	resource rsc = {method, NULL, handler};
	resource_map_[path] = rsc;
}

////////////
//every fd has his own handle_request, because the req and res from http_context
int http_epoll_watcher::handle_request(request& req, response& res)  
{
	std::string uri = req.get_request_uri();
	if(resource_map_.find(uri) == resource_map_.end()) {  //not found
		res.code_msg_ = STATUS_NOT_FOUND;
		res.body_ = STATUS_NOT_FOUND.msg_;
		LOG_INFO("page not found which uri: %s", uri.c_str());
		return 0;
	}

	resource rsc = resource_map_[req.get_request_uri()];

	//check method
	http_method_decorator method = rsc.method_;    
	if(method.get_names()->count(req.line_.get_method()) == 0) {  //only support GET and POST
		res.code_msg_ = STATUS_METHOD_NOT_ALLOWED;
		std::string allow_name = *(method.get_names()->begin());
		res.set_head("Allow", allow_name);

		res.body_.clear();   //cleaer
		res.body_ = STATUS_METHOD_NOT_ALLOWED.msg_;
		
		LOG_INFO("not allow method, allowed: %s, request method: %s",
				 allow_name.c_str(), req.line_.get_method().c_str());
		return 0;
	}

	if(rsc.handle_json_ != NULL) {
		Json::Value root;
		rsc.handle_json_(req, root);
		res.set_body(root);
	}
	else if(rsc.handle_meth_ != NULL) {
		rsc.handle_meth_(req, res);
	}

	LOG_DEBUG("handle response success which code: %d, msg: %s",
			  res.code_msg_.status_code_, res.code_msg_.msg_.c_str());
	return 0;
}

//////////////////////////////////////////

int http_epoll_watcher::on_accept(epoll_context& epoll_ctx)
{
	int connfd = epoll_ctx.connfd_;
	epoll_ctx.ptr_ = new http_context(connfd);
	return 0;
}

int http_epoll_watcher::on_readable(epoll_event& event)
{
	epoll_context* epoll_ctx = static_cast<epoll_context*>(event.data.ptr);
	int fd = epoll_ctx->connfd_;

	int buffer_size = READ_BUFFER_SIZE;
	char read_buff[buffer_size];
	memset(read_buff, 0, buffer_size);

	int read_size = recv(fd, read_buff, buffer_size, 0);
	if((read_size == -1) && (errno == EINTR)) {
		return READ_CONTINUE;
	}
	if((read_size == -1) || (read_size == 0)) {
		return READ_CLOSE;
	}

	LOG_DEBUG("read success which read size: %d", read_size);
	
	http_context* http_ctx = static_cast<http_context*>(epoll_ctx->ptr_);
	if(http_ctx->get_request().parse_part_ == PARSE_REQ_LINE) {
		http_ctx->record_start_time();
	}

	int ret = http_ctx->get_request().parse_request(read_buff, read_size);
	if(ret < 0) {
		return READ_CLOSE;
	}
	else if(ret == NEED_MORE_STATUS) {  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		return READ_CONTINUE;
	}
	
	if(ret == PARSE_LEN_REQUIRED) {
		http_ctx->get_response().code_msg_ = STATUS_LENGTH_REQUIRED;
		http_ctx->get_response().body_ = STATUS_LENGTH_REQUIRED.msg_;
		return READ_OVER;
	}

	//invoke handle_request
	handle_request(http_ctx->get_request(), http_ctx->get_response());

	return READ_OVER;
}

int http_epoll_watcher::on_writeable(epoll_context& epoll_ctx)
{
	int fd = epoll_ctx.connfd_;
	http_context* http_ctx = static_cast<http_context*>(epoll_ctx.ptr_);
	request &req = http_ctx->get_request();
	response &res = http_ctx->get_response();

	bool is_keepalive = (strcasecmp(req.get_header("Connection").c_str(), "keep-alive") == 0);

	if(!res.is_writed_) {
		std::string http_version = req.line_.get_http_version();
		res.gen_response(http_version, is_keepalive);
		res.is_writed_ = true;
	}

	char write_buff[WRITE_BUFFER_SIZE];
	memset(write_buff, 0, WRITE_BUFFER_SIZE);
	int read_size = 0;

	//1. read some response bytes
	int ret = res.readsome(write_buff, WRITE_BUFFER_SIZE, read_size); //read_size is pass by reference

	//2. write bytes to socket
	int nwrite = send(fd, write_buff, read_size, 0);
	if(nwrite <0) {
		LOG_ERROR("send fail, err: %s", strerror(errno));
		return WRITE_CLOSE;
	}

	//3. when not write all buffer, we will rollback write index
	if(nwrite < read_size) {
		res.rollback(read_size - nwrite);
	}

	LOG_DEBUG("send complete which_num: %d, read_size: %d", nwrite, read_size);

	if(ret == 1) {  //not send over
		LOG_DEBUG("has very big response, we will send part first and send other part later ...");
		return WRITE_CONTINUE;
	}

	if((ret == 0) && (nwrite == read_size)) {
		std::string http_method = req.line_.get_method();
		std::string request_url = req.line_.get_request_url();
		int cost_time = http_ctx->get_cost_time();
		LOG_INFO("access_log %s %s status_code: %d cost_time: %d us, body_size: %d, client_ip: %s",
				  http_method.c_str(), request_url.c_str(), res.code_msg_.status_code_, 
				  cost_time, res.body_.size(), epoll_ctx.client_ip_.c_str());
	}

	if(is_keepalive && nwrite > 0) {
		http_ctx->clear();   //delete old request and response, because http is a no_state protocol
		return WRITE_ALIVE;
	}
	
	return WRITE_CLOSE;
}

int http_epoll_watcher::on_close(epoll_context& epoll_ctx)
{
	if(epoll_ctx.ptr_ == NULL) {
		return 0;
	}

	http_context* http_ctx = static_cast<http_context*>(epoll_ctx.ptr_);
	if(http_ctx != NULL) {
		delete http_ctx;
		http_ctx = NULL;
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////
//http_server

http_server::http_server() 
	: backlog_(100000), max_events_(1000), port_(3456), tid_(0)
{
}

/////////////////////////////////////////

int http_server::start(int port, int backlog, int max_events)
{
	LOG_WARN("start() method is deprecated, please use start_sync() or start_async() instead!");
	ep_socket_.set_port(port);
	ep_socket_.set_backlog(backlog);
	ep_socket_.set_max_events(max_events);
	ep_socket_.set_watcher(&http_handler_);
	return ep_socket_.start_epoll();
}

void* http_start_routinue(void* ptr)
{
	http_server *server = static_cast<http_server*>(ptr);
	server->start_sync();
	return static_cast<void*>(0);
}

int http_server::start_async()
{
	int ret = pthread_create(&tid_, NULL, http_start_routinue, this);
	if(ret != 0) {
		LOG_ERROR("http_server::start_async err: %d", ret);
		return ret;
	}
	return 0;
}

int http_server::join()
{
	if(tid_ == 0) {
		LOG_ERROR("http_server not start async");
		return -1;
	}
	return pthread_join(tid_, NULL);
}

int http_server::start_sync()
{
	ep_socket_.set_port(port_);
	ep_socket_.set_backlog(backlog_);
	ep_socket_.set_max_events(max_events_);
	ep_socket_.set_watcher(&http_handler_);
	return ep_socket_.start_epoll();
}

int http_server::stop()
{
	LOG_INFO("stoping http server...");
	return ep_socket_.stop_epoll();
}

//////////////////////////////////////////////////

void http_server::add_mapping(std::string path, method_handler handler, http_method_decorator method)
{
	http_handler_.add_mapping(path, handler, method);
}

void http_server::add_mapping(std::string path, json_handler handler, http_method_decorator method)
{
	http_handler_.add_mapping(path, handler, method);
}

void http_server::add_bind_ip(std::string ip)
{
	ep_socket_.add_bind_ip(ip);
}

void http_server::set_thread_pool(thread_pool* tp)
{
	ep_socket_.set_thread_pool(tp);
}

void http_server::set_backlog(int backlog)
{
	backlog_ = backlog;
}

void http_server::set_max_events(int max_ev)
{
	max_events_ = max_ev;
}

void http_server::set_port(int port)
{
	port_ = port;
}

