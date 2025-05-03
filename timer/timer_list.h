#ifndef TIMER_LIST_H_
#define TIMER_LIST_H_

#include "../http.h"
#include "../log/log.h"
#include "../log/block_queue.h"
#include <bits/types/time_t.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sys/epoll.h>


class util_timer;

//用户的数据结构
struct client_data{
    sockaddr_in address;
    int sockfd;
    util_timer *timer;         //为每一个用户创建一个定时器
};

//定时器节点
class util_timer{
    public:
        time_t expire;                          //超时时间

        void (*cb_func)(client_data *);         //超时处理的函数指针
        client_data *user_data;
        util_timer *prev;
        util_timer *next;
    public:
        util_timer():prev(NULL), next(NULL){}
};

//定时器容器的具体操作，该容器采用升序排序
class sort_timer_list{
    private:
        util_timer *head;
        util_timer *tail;
        void add_timer(util_timer *timer, util_timer *list_head);
    public:
        sort_timer_list();
        ~sort_timer_list();

        void add_timer(util_timer *timer);
        void del_timer(util_timer *timer);
        void adjust_timer(util_timer *timer);
        void tick();
};

//超时处理操作
class timeout_process{
    public:
        static int *pipefd_;       //socketpipe 
        sort_timer_list timer_list_;
        static int epollfd_;
        int TIMESLOT_;              //定时基值

    public:
        timeout_process(){};
        ~timeout_process(){};

        void init(int timeslot);
        int setnonblocking(int fd);
        //选择ET模式时需要开启one_shot模式
        void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);
        //************************这里的信号处理函数为何要设置成静态函数？
        static void sig_handler(int sig);
        //设置信号,这里的第二个参数在c++中会隐式转换为void(handler*)(int)函数指针类型,这种写法更加清晰
        void addsig(int sig, void(handler)(int), bool restar = true);
        //定时处理
        void timer_handler();
        void show_error(int connfd, const char *info);
};

void cb_func(client_data *user_data);

#endif
