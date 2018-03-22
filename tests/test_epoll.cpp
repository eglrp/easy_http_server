#include "epoll_socket.h"

#include <sys/socket.h>

class echo_watcher : public epoll_socket_watcher {
public:
	virtual int on_accept(epoll_context& epoll_ctx) {
		return 0;
	}
	virtual int on_readable(epoll_event& event) {
		epoll_context* epoll_ctx = (epoll_context*)(event.data.ptr);
		int fd = epoll_ctx->connfd_;
		memset(buff_, '\0', 1024);
		int ret = recv(fd, buff_, 1024, 0);
		if(ret == 0)
			return READ_CLOSE;
		printf("received: %s\n", buff_);
		return READ_OVER;
	}
	virtual int on_writeable(epoll_context& epoll_ctx) {
		int fd = epoll_ctx.connfd_;
		send(fd, buff_, strlen(buff), 0);
		printf("send: %s\n", buff_);
		return WRITE_ALIVE;
	}
	virtual int on_close(epoll_context& epoll_ctx) {
		printf("bye\n");
		return 0;
	}
private:
	char buff_[1024];
};

class echo_server {
public:
	echo_server(int bl, int max_ev, int pt) : backlog_(bl), max_events_(max_ev), port_(pt)
	{}
	
	int start() {
		ep_socket_.set_port(port_);
		ep_socket_.set_backlog(backlog_);
		ep_socket_.set_max_events(max_events_);
		ep_socket_.set_watcher(&echo_handler_);
		return ep_socket_.start_epoll();
	}	
private:
	int           backlog_;
	int			  max_events_;
	int 		  port_;
	echo_watcher  echo_handler_;
	epoll_socket  ep_socket_;
};


#include <string.h>

int main()
{
	echo_server server(5, 1024, 6666);
	server.start();
	return 0;
}
