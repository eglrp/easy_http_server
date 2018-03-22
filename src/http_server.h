#ifndef _HTTP_SERVER_H
#define _HTTP_SERVER_H

#include "epoll_socket.h"
#include "http_context.h"

#include <sys/epoll.h>

#include <string>
#include <map>
#include <set>

class http_method_decorator {
public:
	http_method_decorator() = default;
	http_method_decorator(int code, std::string name);
	http_method_decorator& operator|(http_method_decorator hm);
public:
	std::set<int>* 		   get_codes();
	std::set<std::string>* get_names();
private:
	std::set<int> 		  codes_;
	std::set<std::string> names_;
};

static http_method_decorator GET_METHOD = http_method_decorator{1, "GET"};
static http_method_decorator POST_METHOD = http_method_decorator{2, "POST"};

typedef void (*method_handler)(request& req, response& res);
typedef void (*json_handler)(request& req, Json::Value& res);

struct resource {
	http_method_decorator    method_;
	method_handler 			 handle_meth_;
	json_handler   			 handle_json_;
};

class http_epoll_watcher : public epoll_socket_watcher {
public:
	virtual int on_accept(epoll_context& epoll_ctx);
	virtual int on_readable(epoll_event& event);   //create epoll context
	virtual int on_writeable(epoll_context& epoll_ctx);
	virtual int on_close(epoll_context& epoll_ctx);
public:
	void add_mapping(std::string parh, method_handler handler, http_method_decorator method = GET_METHOD);
	void add_mapping(std::string path, json_handler handler, http_method_decorator = GET_METHOD);
	int  handle_request(request& req, response& res);
private:
	std::map<std::string, resource> resource_map_;
};

class http_server {
public:
	http_server();
	//default dtor is okay
public:
	int start(int port, int backlog = 10, int max_events = 1000);
	int start_async();  //if start by async, you need to invoke join()
	int start_sync();
	int join();
	int stop();
	
	void set_thread_pool(thread_pool* tp);
	void set_backlog(int backlog);
	void set_max_events(int max_ev);
	void set_port(int port);
	void add_bind_ip(std::string ip);
public:  //warpper function
	void add_mapping(std::string path, method_handler handler, http_method_decorator method = GET_METHOD);
	void add_mapping(std::string path, json_handler handler, http_method_decorator = GET_METHOD);
private:
	http_epoll_watcher http_handler_;
	epoll_socket       ep_socket_;
	int                backlog_;
	int                max_events_;
	int                port_;
	pthread_t          tid_;    //when start sync
};

#endif
