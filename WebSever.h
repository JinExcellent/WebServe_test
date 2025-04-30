#ifndef WEBSERVER_H_
#define WEBSERVER_H_

#include "http.h"
#include "mysql/sql_connection_pool.h"
#include "threadpool.h"
#include "./timer/timer_list.h"
#include <netinet/in.h>
#include <sys/epoll.h>

const int MAX_FD = 65535;               //最大文件描述符
const int MAX_EVENT_NUMBER = 10000;     //支持最大的I/O并发数
const int TIMESLOT = 5;


class WebServer{
    //************这里的一个疑问，如果是对服务器的整体运行框架进行封装，这里的变量都设置为public是否有些不妥？
    public:
        int port_;              //端口号
        char *root_;            //文件资源路径
        int log_write_;
        int close_log_;         //是否开始日志
        int actor_model_;       //服务器并发模式
        
        int pipefd_[2];
        int epollfd_;
        http_conn *users;       //用户连接队列

        //数据库相关
        connection_pool *connpool_;      //连接池指针
        string username_;
        string password_;
        string databasename_;
        int sql_num_;

        //线程池相关
        threadpool<http_conn> *pool_;   //线程池指针
        int thread_num_;

        //io复用相关
        epoll_event events_[MAX_EVENT_NUMBER];

        //定时器相关
        client_data *users_timer_;
        timeout_process timeout_;        //超时处理的定时器

        int listenfd_;
        int OPT_LINGER_;
        int LISTENTrigmod_;
        int TRIGMode_;                 //epoll所使用的模式
        int CONNTrigmode_;

    public:
        WebServer();
        ~WebServer();

        void init(int port, string user, string passWord, string databaseName, int log_write, int opt_linger, int sql_num, int trigmode, int thread_num, int close_log, int actor_model);
        void thread_pool();
        void sql_pool();
        void log_write();
        void trig_mode();
        void eventListen();
        void eventLoop();
        void timer(int connfd, struct sockaddr_in client_address);
        void adjust_timer(util_timer *timer);
        void deal_timer(util_timer *timer, int sockfd);
        bool dealclientdata();
        bool dealwithsignal(bool& timeout, bool& stop_server);
        void dealwithread(int sockfd);
        void dealwithwrite(int sockfd);
};
#endif
