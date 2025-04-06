#ifndef HTTP_H_
#define HTTP_H_

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

class http_conn{
    private:
        int sockfd_;
        sockaddr_in addr_;

    public:
        static int epollfd_;        //被所有socket上的事件
        static int epollfd_count_;  //用户数量
    public:
        void init(int fd, sockaddr_in addr);
        void close_conn();

};
#endif
