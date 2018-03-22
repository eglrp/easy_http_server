#include "epoll_socket.h"
#include "log.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <sys/sysinfo.h>

epoll_socket::epoll_socket() 
	: backlog_(100000), max_events_(1000), port_(-1), 
	  epollfd_(-1), clients_(0), thread_pool_(NULL), 
	  use_default_thread_pool_(true), status_(EP_RUN)
{
}

epoll_socket::~epoll_socket()
{
	if((thread_pool_ != NULL) && use_default_thread_pool_) {
		delete thread_pool_;
		thread_pool_ = NULL;
	}
}

int epoll_socket::stop_epoll()
{
	status_ = EP_REJECT_CONN;
	LOG_INFO("stop epoll, current client: %u", clients_);
	return 0;
}

int epoll_socket::start_epoll()
{
	int ret = init_thread_pool();
	CHECK(ret, "thread pool start error: %d", ret);

	ret = listen_on();
	CHECK(ret, "listen err: %d", ret);

	ret = create_epoll();
	CHECK(ret, "create epoll err: %d", ret);

	ret = add_listen_sock_to_epoll();
	CHECK(ret, "add listen sock to epoll fail: %d", ret);

	return start_event_loop();
}

//1
int epoll_socket::init_thread_pool() 
{
	if(thread_pool_ == NULL)
		init_default_thread_pool();
	
	int ret = thread_pool_->start();
	return ret;
}

int epoll_socket::init_default_thread_pool()
{
	int core_num = get_nprocs();
	LOG_INFO("thread pool not set, we will build for core size: %d", core_num);
	thread_pool_ = new thread_pool();
	thread_pool_->set_pool_size(core_num);
	return 0;
}

//2
int epoll_socket::listen_on()
{
	if(bind_ips_.empty()) {
		int ret = bind_on(INADDR_ANY);
		if(ret != 0)
			return ret;
		LOG_INFO("bind for all ip (0.0.0.0)!");
	}
	else {
		const int size = bind_ips_.size();
		for(int i=0; i<size; ++i) {
			unsigned int ip = inet_addr(bind_ips_[i].c_str());
			int ret = bind_on(ip);
			if(ret != 0)
				return ret;
			LOG_INFO("bind for ip: %s success", bind_ips_[i].c_str());
		}
	}

	LOG_INFO("start server socket on port: %d", port_);
	return 0;
}

int epoll_socket::bind_on(unsigned int ip)
{
	//listen on sock_fd, new connection on new_fd
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1) {
		LOG_ERROR("socket error: %s", strerror(errno));
		return -1;
	}

	int on = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	struct sockaddr_in my_addr;   //my address information
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;    //host byte order
	my_addr.sin_port = htons(port_);
	my_addr.sin_addr.s_addr = ip;

	if(bind(sockfd, reinterpret_cast<struct sockaddr*>(&my_addr), sizeof(my_addr)) == -1) {
		LOG_ERROR("bind error: %s", strerror(errno));
		return -1;
	}

	if(listen(sockfd, backlog_) == -1) {
		LOG_ERROR("listen error: %s", strerror(errno));
		return -1;
	}
	listen_sockets_.insert(sockfd);   //insert the listenfd to std::set<int> listen_sockets
	return 0;
}

//3
int epoll_socket::create_epoll()
{
	epollfd_ = epoll_create(32000);
	if(epollfd_ == -1) {
		LOG_ERROR("epoll_create: %s", strerror(errno));
		return -1;
	}
	return 0;
}

//4
int epoll_socket::add_listen_sock_to_epoll() 
{
	for(auto sockfd : listen_sockets_) {
		struct epoll_event ev;
		ev.events = EPOLLIN;
		ev.data.fd = sockfd;
		if(epoll_ctl(epollfd_, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
			LOG_ERROR("epoll_ctl: listen_sock: %s", strerror(errno));
			return -1;
		}
	}
	return 0;
}
//5
int epoll_socket::start_event_loop()
{
	epoll_event *events = new epoll_event[max_events_];
	while(status_ != EP_STOP) {
		int number = epoll_wait(epollfd_, events, max_events_, -1);
		if(number == -1) {
			if(errno == EINTR) 
				continue;

			LOG_ERROR("epoll_wait error: %s", strerror(errno));
			break;
		}

		for(int i=0; i<number; ++i)
			handle_event(events[i]);
	}
	LOG_INFO("epoll wait loop stop");
	if(events != NULL) {
		delete[] events;
		events = NULL;
	}

	return 0;
}

void read_func(void* data)  //thread pool thsk function
{
	task_data* td = static_cast<task_data*>(data);
	td->ep_socket_->handle_readable_event(td->event_);  //multi thread handle readable event
	delete td;
}

//then do other things
int epoll_socket::handle_event(epoll_event& event)
{
	if(listen_sockets_.count(event.data.fd)) {  //if listenfd
		//acception connection
		if(status_ == EP_RUN) {
			handle_accept_event(event);
		}
		else {
			LOG_INFO("current status: %d, not accept new connect", status_);
			{
				lock_guard lock(mutex_);
				if((clients_ == 0) && (status_ == EP_REJECT_CONN)) {
					status_ = EP_STOP;
					LOG_INFO("client is empty and ready for stop server!");
				}
			}
		}
	}
	else if(event.events & EPOLLIN) {  //handle readable async
		LOG_DEBUG("start handle readable event");

		task_data* tdata = new task_data();
		tdata->event_ = event;
		tdata->ep_socket_ = this;

		task* tk = new task(read_func, tdata);

		int ret = thread_pool_->add_task(tk);
		if(ret != 0) {
			LOG_WARN("add read task fail: %d, we will close connect.", ret);
			close_and_release(event);
			delete tdata;
			delete tk;
		}
	}
	else if(event.events & EPOLLOUT) {
		handle_writeable_event(event);
	}
	else {
		LOG_INFO("unknow events : %d", event.events);
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
//1. accept
int epoll_socket::handle_accept_event(epoll_event& event)
{
	int sockfd = event.data.fd;
	
	std::string client_ip;
	int conn_sock = accept_socket(sockfd, client_ip);
	if(conn_sock == -1)
		return -1;
	
	set_nonblocking(conn_sock);     //set nonblocking

	{
		lock_guard lock(mutex_);
		++clients_;
	}
	LOG_DEBUG("get accept socket whick listen fd: %d, conn_sock_fd: %d", sockfd, conn_sock);

	epoll_context *epoll_ctx = new epoll_context();
	epoll_ctx->connfd_ = conn_sock;
	epoll_ctx->client_ip_ = client_ip;

	//invoke the watcher's on_accept
	watcher_->on_accept(*epoll_ctx);

	struct epoll_event conn_sock_ev;
	conn_sock_ev.events = EPOLLIN | EPOLLONESHOT;
	conn_sock_ev.data.ptr = epoll_ctx;

	if(epoll_ctl(epollfd_, EPOLL_CTL_ADD, conn_sock, &conn_sock_ev) == -1) {
		LOG_ERROR("epoll_ctl : conn_sock: %s", strerror(errno));
		return -1;
	}
	return 0;
}

int epoll_socket::accept_socket(int sockfd, std::string& client_ip)
{
	int new_fd;
	struct sockaddr_in client_addr;
	socklen_t len = sizeof(client_addr);
	
	if((new_fd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&client_addr), &len)) == -1) {
		LOG_ERROR("accept error: %s", strerror(errno));
		return -1;
	}

	client_ip = inet_ntoa(client_addr.sin_addr);
	LOG_DEBUG("server: got connection from %s\n", client_ip.c_str());
	return new_fd;
}

int epoll_socket::set_nonblocking(int fd)
{
	int option;
	if((option = fcntl(fd, F_GETFL, 0)) == -1)
		option = 0;
	return fcntl(fd, F_SETFL, option | O_NONBLOCK);
}


///////////////////////////////////////////////////////////////////////////////////////////////
// 2. read !
int epoll_socket::handle_readable_event(epoll_event& event)
{
	//epoll_context packed the fd, clientip, request, and responce.
	epoll_context* epoll_ctx = static_cast<epoll_context*>(event.data.ptr);
	int fd = epoll_ctx->connfd_;

	//invoke the watcher's on_readable
	int ret = watcher_->on_readable(event);

	if(ret == READ_CLOSE) {
		return close_and_release(event);
	}
	if(ret == READ_CONTINUE) {   //READCONTINUE                   READCONTINUE !!!!!!!!!!!!!!!!!!!
		event.events = EPOLLIN | EPOLLONESHOT;
		epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &event);
	}
	else if(ret == READ_OVER) {
		event.events = EPOLLOUT | EPOLLONESHOT;
		epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &event);
	}
	else {
		LOG_ERROR("unknow read ret: %d", ret);
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// 3. write !
int epoll_socket::handle_writeable_event(epoll_event& event)
{
	epoll_context* epoll_ctx = static_cast<epoll_context*>(event.data.ptr);
	int connfd = epoll_ctx->connfd_;
	LOG_DEBUG("start write data");

	//invoke watcher's on writeable
	int ret = watcher_->on_writeable(*epoll_ctx);

	if(ret == WRITE_CLOSE) {
		return close_and_release(event);
	}

	if(ret == WRITE_CONTINUE) {
		event.events = EPOLLOUT | EPOLLONESHOT;
	}
	else if(ret == WRITE_ALIVE) {
		if(status_ == EP_REJECT_CONN) {
			return close_and_release(event);
		}
		
		//wait for next request
		event.events = EPOLLIN | EPOLLONESHOT;   //reinsert
	}
	else {
		LOG_ERROR("unknow write ret: %d", ret);
	}

	epoll_ctl(epollfd_, EPOLL_CTL_MOD, connfd, &event);

	return 0;
}

int epoll_socket::close_and_release(epoll_event& event)
{
	if(event.data.ptr == NULL) 
		return 0;
	
	LOG_DEBUG("connect close");

	epoll_context *epoll_ctx = static_cast<epoll_context*>(event.data.ptr);
	
	//invoke watcher's on_close
	watcher_->on_close(*epoll_ctx);

	int fd = epoll_ctx->connfd_;
	event.events = EPOLLIN | EPOLLOUT;
	epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &event);

	delete static_cast<epoll_context*>(event.data.ptr);
	event.data.ptr = NULL;

	int ret = close(fd);

	{
		lock_guard lock(mutex_);
		--clients_;
		if((clients_ == 0) && (status_ == EP_REJECT_CONN)) {
			status_ = EP_STOP;
			LOG_INFO("client is empty and ready for stop server!");
		}
	}
	
	LOG_DEBUG("connect close complete which fd: %d, ret: %d", fd, ret);
	return ret;
}

