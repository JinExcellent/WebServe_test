#include "http.h"
#include <asm-generic/socket.h>
#include <sys/socket.h>

int http_conn::epollfd_ = -1;
int http_conn::epollfd_count_ = 0;

int setnonblocking(int fd){
    int old_flags = fcntl(fd, F_GETFL);
    int new_flags = old_flags | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flags);
    
    return  old_flags;
}

//向epoll描述符集中添加描述符
void addfd(int epollfd, int fd, bool one_shot){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;

    if(one_shot){
        //避免同一个通信被不同的线程处理
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    //设置描述符位非阻塞
    setnonblocking(fd);
}

void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//重置描述符，确保下一次可读时，EPOLLIN事件被激活
void modfd(int epollfd, int fd, int ev){
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void http_conn::init(int fd, sockaddr_in addr){
   sockfd_ = fd;
   addr_ = addr;
    
   int reuse = true;
   setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
   addfd(epollfd_, sockfd_, true);
   epollfd_count_++;

}

void http_conn::close_conn(){
    if(sockfd_ != -1){
        removefd(epollfd_, sockfd_);
        sockfd_ = -1;
        epollfd_count_--;
    }
}


















