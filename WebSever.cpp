#include "WebSever.h"

WebServer::WebServer(){
    //申请用户请求队列空间
    users = new http_conn[MAX_FD];
   
    //获取资源文件绝对路径
    char server_path[200];
    getcwd(server_path, sizeof(server_path));
    char root[6] = "root/";
    root_ = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(root_, server_path);
    strcat(root_, root);
    //申请定时器队列空间
    users_timer_ = new client_data[MAX_FD];
}

WebServer::~WebServer(){
    close(epollfd_);
    close(listenfd_);
    close(pipefd_[0]);
    close(pipefd_[1]);
    delete [] users;
    delete [] pool_;
    delete [] users_timer_;
}

void WebServer::init(int port, string user, string passWord, string databaseName, int log_write, int opt_linger, int sql_num, int trigmode, int thread_num, int close_log, int actor_model){
    port_ = port;
    username_ = user;
    password_ = passWord;
    databasename_ = databaseName;
    sql_num_ = sql_num;
    thread_num_ = thread_num;
    log_write_ = log_write; 
    close_log_ = close_log;
    OPT_LINGER_ = opt_linger;
    TRIGMode_ = trigmode;
    actor_model_ = actor_model;
 }

void WebServer::trig_mode(){
    //选项为0使用LT，否则使用ET；
    if(TRIGMode_ == 0){
        LISTENTrigmod_ = 0;
        CONNTrigmode_ = 0;
    }
    else if(TRIGMode_ == 1){
        LISTENTrigmod_ = 0;
        CONNTrigmode_ = 1;
    }
    else if(TRIGMode_ == 2){
        LISTENTrigmod_ = 1;
        CONNTrigmode_ = 0;
    }
    else if(TRIGMode_ == 3){
        LISTENTrigmod_ = 1;
        CONNTrigmode_ = 1;
    }
}

void WebServer::log_write(){
    if(close_log_ == 0){
        if(log_write_ == 1)     //异步写
            log::get_instance()->init("./ServerLog", close_log_, 2000, 5000, 500);
        else        //同步写
            log::get_instance()->init("./ServerLog", close_log_, 2000, 5000, 0);
    }
}

void WebServer::sql_pool(){
    //初始化连接池
    connpool_ = connection_pool::GetInstance();
    connpool_->init("localhost", username_, password_, databasename_, 3036, sql_num_, close_log_);
    //初始化用户数据库表
    users -> initmysql_result(connpool_);
}

void WebServer::thread_pool(){
    //线程池中的代码需要进行修改*************************
    pool_ = new threadpool<http_conn>(actor_model_, connpool_, thread_num_);
}

void WebServer::eventListen(){
    listenfd_ = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd_ > 0);

    //优雅关闭连接
    if(OPT_LINGER_ == 0){
       struct linger temp = {0,1};
       setsockopt(listenfd_, SOL_SOCKET, SO_LINGER, &temp, sizeof(temp));
    }
    else if(OPT_LINGER_ == 1){
       struct linger temp = {1,1};
       setsockopt(listenfd_, SOL_SOCKET, SO_LINGER, &temp, sizeof(temp));
    }

    struct sockaddr_in sever_addr;
    memset(&sever_addr, 0, sizeof(sever_addr));
    sever_addr.sin_family = AF_INET;
    sever_addr.sin_port = htons(port_);
    sever_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //地址端口号复用
    int flag = 1;
    setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    int ret = 0;
    ret = bind(listenfd_, (struct sockaddr *)&sever_addr, sizeof(sever_addr));
    assert(ret >= 0);
    assert((ret = listen(listenfd_, 5)) >= 0);
    
    //初始化定时器
    timeout_.init(TIMESLOT);
    
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd_ = epoll_create(5);
    assert(epollfd_ != -1);
   
    //设置监听描述符的模式
    timeout_.addfd(epollfd_, listenfd_, false, LISTENTrigmod_);
    http_conn::epollfd_ = epollfd_;

    //创建socket读写管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd_);
    assert(ret != -1);
    timeout_.setnonblocking(pipefd_[1]);
    timeout_.addfd(epollfd_, pipefd_[0], false, 0);

    //设置信号
    timeout_.addsig(SIGPIPE, SIG_IGN);
    timeout_.addsig(SIGALRM, timeout_.sig_handler, false);
    timeout_.addsig(SIGTERM, timeout_.sig_handler, false);
    //定时
    alarm(TIMESLOT);

    //赋值timeout_process类中两个静态变量
    timeout_process::epollfd_ = epollfd_;
    timeout_process::pipefd_ = pipefd_;
}

void WebServer::timer(int connfd, struct sockaddr_in client_address){
   //初始化连接队列中（使用连接描述符作为索引）每一个用户的http连接
    users[connfd].init(connfd, client_address, root_, CONNTrigmode_, close_log_, username_, password_, databasename_);

   //初始化定时队列中用户的数据结构
    users_timer_[connfd].address = client_address;
    users_timer_[connfd].sockfd = connfd;
    //创建定时器节点
    util_timer *timer = new util_timer;
    //将创建的节点绑定到用户数据结构并设置信号处理函数
    timer->user_data = &users_timer_[connfd];
    timer->cb_func = cb_func;       //这里的cb_func函数非类成员函数  
    time_t curr = time(NULL);
    timer->expire = curr + 3 * TIMESLOT;
    users_timer_[connfd].timer = timer;
    timeout_.timer_list_.add_timer(timer);
}

void WebServer::adjust_timer(util_timer *timer){
    time_t curr = time(NULL);
    timer->expire = curr + 3 * TIMESLOT;
    timeout_.timer_list_.adjust_timer(timer);
   
    LOG_INFO("adjust timer once:%d", timer->user_data->sockfd);
}

void WebServer::deal_timer(util_timer *timer, int sockfd){
  timer->cb_func(&users_timer_[sockfd]);  
  if(timer){
        timeout_.timer_list_.del_timer(timer);
  }
  LOG_INFO("closed fd %d", users_timer_[sockfd].sockfd);
}

bool WebServer::dealclientdata(){
   struct sockaddr_in client_addr;
   socklen_t clientaddr_len = sizeof(client_addr);

   //监听描述符为LT模式
   if(LISTENTrigmod_ == 0){
        int connfd = accept(listenfd_, (struct sockaddr *)&client_addr, &clientaddr_len);

        if(connfd < 0){
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if(http_conn::epollfd_count_ > MAX_FD){
            timeout_.show_error(connfd, "Internal server busy.");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_addr);
   }else{
        while(1){
            int connfd = accept(listenfd_, (struct sockaddr *)&client_addr, &clientaddr_len);
            if(connfd < 0){
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if(http_conn::epollfd_count_ > MAX_FD){
                timeout_.show_error(connfd, "Internal server busy.");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_addr);
        }
        return false;
   }
   return true;
}

bool WebServer::dealwithsignal(bool &timeout, bool &stop_server){
    int ret = 0;
    int sig;
    char signals[1024];
    
    ret = recv(pipefd_[0], signals, sizeof(signals), 0);
    if(ret == -1){
        return false;
    }
    else if(ret == 0){
        return false;
    }else{
        for(int i = 0; i < ret; i++){
            switch (signals[i]) {
                case SIGALRM:
                    timeout = true;
                    break;
                case SIGTERM:
                    stop_server = true;
                    break;
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd){
   util_timer *timer = users_timer_[sockfd].timer; 
    //reactor
   if(actor_model_ == 1){
        if(timer){
            adjust_timer(timer);
        }
        //reactor模式中，主线程只需要将事件添加到请求队列中
        pool_->append(users + sockfd, 0);

        //*************此处的代码需要通过调试搞懂*************
        while(true){
            if(users[sockfd].improv_ == 1){
                if(users[sockfd].timer_flag_ == 1){
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag_ = 0;
                }
                users[sockfd].improv_ = 0;
                break;
            }
        }
   }else{
    //proactor
        //*************此处的代码需要通过调试搞懂*************
        if(users[sockfd].read()){
            LOG_INFO("deal with the client[%s]", inet_ntoa(users[sockfd].get_address()->sin_addr));        
            pool_->append(users + sockfd);

            if(timer)
                adjust_timer(timer);
        }else
            deal_timer(timer, sockfd);
   }
}

void WebServer::dealwithwrite(int sockfd){
   util_timer *timer = users_timer_[sockfd].timer;
    //reactor
   if(actor_model_ == 1){
        if(timer){
            adjust_timer(timer);
        }
        //reactor模式中，主线程只需要将事件添加到请求队列中
        pool_->append(users + sockfd, 1);

        //*************此处的代码需要通过调试搞懂*************
        while(true){
            if(users[sockfd].improv_ == 1){
                if(users[sockfd].timer_flag_ == 1){
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag_ = 0;
                }
                users[sockfd].improv_ = 0;
                break;
            }
        }
   }else{
    //proactor
        //*************此处的代码需要通过调试搞懂*************
        if(users[sockfd].write()){
            LOG_INFO("send data to the client[%s]", inet_ntoa(users[sockfd].get_address()->sin_addr));        
            pool_->append(users + sockfd);

            if(timer)
                adjust_timer(timer);
        }else
            deal_timer(timer, sockfd);
 
    }
}

void WebServer::eventLoop(){
    bool timeout = false;
    bool stop_server = false;
    
    while(!stop_server){
        int number = epoll_wait(epollfd_, events_, MAX_EVENT_NUMBER, -1);
        if(number < 0 && errno != EINTR){
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for(int i = 0; i < number; i++){
           int sockfd = events_[i].data.fd;

           if(sockfd == listenfd_){
                bool flag = dealclientdata();
                if(flag == false)
                    continue;
           }
           else if(events_[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
               //关闭服务端连接，删除对应的定时器节点
               util_timer *timer = users_timer_[i].timer;
               deal_timer(timer, sockfd);
           }
           else if((sockfd == pipefd_[0]) && (events_[i].events & EPOLLIN)){
                bool flag = dealwithsignal(timeout, stop_server);
                if(flag == false)
                    LOG_ERROR("%s", "dealsignal failure");
           }
           else if(events_[i].events & EPOLLIN)
               dealwithread(sockfd);
           else if(events_[i].events & EPOLLOUT)
               dealwithwrite(sockfd);
        }
        //由于对于节点的处理优先级较低，故放在最后
        //if(timeout){
        if(0){
            timeout_.timer_handler(); 
            LOG_INFO("%s", "deal with timeout clocks");

            timeout = false;
        }
    }
}
