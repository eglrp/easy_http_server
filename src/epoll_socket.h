#ifndef _EPOLL_SOCKET_H
#define _EPOLL_SOCKET_H

#include "thread_pool.h"
#include "utili.h"

#include <sys/epoll.h>
#include <vector>
#include <set>
#include <string>

//epoll socket status
const int EP_RUN = 0;
const int EP_REJECT_CONN = 1;
const int EP_STOP = 2;

//write connfd state
const int WRITE_ALIVE = 0;  //keep alive
const int WRITE_CLOSE = 1;
const int WRITE_CONTINUE = 2;

//read state
const int READ_OVER = 0;
const int READ_CONTINUE = 1;
const int READ_CLOSE = -1;

//buffer size
const int WRITE_BUFFER_SIZE = 4096;
const int READ_BUFFER_SIZE = 4096;

struct epoll_context {
	void* 		ptr_;
	int   		connfd_;
	std::string client_ip_;
};

class epoll_socket;

struct task_data {
	epoll_event   event_;
	epoll_socket* ep_socket_;
};

class epoll_socket_watcher {
public:
	virtual ~epoll_socket_watcher() {}
	virtual int on_accept(epoll_context& ep_ctx) = 0;
	virtual int on_readable(epoll_event& event) = 0;  //create epoll context
	virtual int on_writeable(epoll_context& ep_ctx) = 0; //return 1->WRITE_CLOSE, return 2->WRITE_CONTINUE
	virtual int on_close(epoll_context& ep_ctx) = 0;
};

class epoll_socket {
public:
	epoll_socket();
	~epoll_socket();
public:
	int  handle_readable_event(epoll_event& event);
	int  start_epoll();
	int  stop_epoll();
	void set_thread_pool(thread_pool* tp);
	void set_port(int port);
	void set_watcher(epoll_socket_watcher* w);
	void set_backlog(int backlog);
	void set_max_events(int max_events);
	void add_bind_ip(std::string ip);
private:
	int  set_nonblocking(int fd);
	int  accept_socket(int sockfd, std::string& client_ip);
	int  bind_on(unsigned int ip);
	int  listen_on();
	int  handle_accept_event(epoll_event& event);
	int  handle_writeable_event(epoll_event& event);
	int  close_and_release(epoll_event& event);
	int  init_default_thread_pool();
	int  init_thread_pool();
	int  create_epoll();
	int  add_listen_sock_to_epoll();
	int  handle_event(epoll_event& event);
	int  start_event_loop();
private:
	std::vector<std::string> bind_ips_;
	int                      backlog_;
	int                      max_events_;
	int                      port_;
	int                      epollfd_;
	std::set<int>            listen_sockets_;
	mutex                    mutex_;
	volatile int             clients_;
	thread_pool*             thread_pool_;
	bool                     use_default_thread_pool_;
	volatile int             status_;
	epoll_socket_watcher*    watcher_;
};

inline void epoll_socket::set_thread_pool(thread_pool *tp) //user-defined thread_pool
{
	thread_pool_ = tp;
	use_default_thread_pool_ = false;
}

inline void epoll_socket::set_port(int port)
{
	port_ = port;
}

inline void epoll_socket::set_watcher(epoll_socket_watcher* w)
{
	watcher_ = w;
}

inline void epoll_socket::set_backlog(int backlog)
{
	backlog_ = backlog;
}

inline void epoll_socket::set_max_events(int max)
{
	max_events_ = max;
}

inline void epoll_socket::add_bind_ip(std::string ip)
{
	bind_ips_.push_back(ip);
}


#endif
