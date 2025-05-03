#include "timer_list.h"

sort_timer_list::sort_timer_list(){
    head = NULL;
    tail = NULL;
}

sort_timer_list::~sort_timer_list(){
    util_timer *temp = head;

    while(temp){
        head = temp->next;
        delete temp;
        temp = head;
    }
}

void sort_timer_list::add_timer(util_timer *timer){
    if(!timer)
        return;
    if(!head){
        head = tail = timer;
        return;
    }
    if(timer->expire < head->expire){
        timer->next = head;
        head->prev = timer;
        head = timer;

        return;
    }
    //调用私有部分的重载函数
    add_timer(timer, head);
}

void sort_timer_list::adjust_timer(util_timer *timer){
    if(!timer)
        return;

    util_timer *temp = timer->next;
    if(!temp || (timer->expire < temp->expire))
        return;

    if(timer == head){
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        
        add_timer(timer, head);
    }else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        //注意这里第二个参数传入的不是头节点
        add_timer(timer, timer->next);
    }
}

void sort_timer_list::del_timer(util_timer *timer){
    if(!timer)
        return;

    if((timer == head) && (timer == tail)){
        delete timer;
        head = NULL;
        tail = NULL;

        return;
    }

    if(timer == head){
        head = head->next;
        head->prev = NULL;
        delete timer;

        return;
    }

    if(timer == tail){
        tail = tail->prev;
        tail->next = NULL;
        delete timer;

        return;
    }
    timer->next->prev = timer->prev;
    timer->prev->next = timer->next;
    delete timer;
}

void sort_timer_list::tick(){
    if(!head)
        return;

    time_t current = time(NULL);
    util_timer *temp = head;

    while(temp){
        //判断是否超时
        if(current < temp->expire){
            break;
        }
        temp->cb_func(temp->user_data);
        head = temp->next;
        if(head)
            head->prev = NULL;
        delete temp;        //删除超时连接节点
        temp = head;
    }
}

void sort_timer_list::add_timer(util_timer *timer, util_timer *list_head){
    util_timer *prev = list_head;
    util_timer *temp = prev->next;

    while(temp){
        if(timer->expire < temp->expire){
            prev->next = timer;
            timer->next = temp;
            temp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = temp;
        temp = temp->next;
    }
    if(!temp){
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}


int timeout_process::epollfd_ = 0;
int *timeout_process::pipefd_ = 0;

class timeout_process;      //这是是为了允许在 cb_func 函数中使用 Utils 类的静态成员而设置的前向声明，而不是为了其他目的。

void timeout_process::init(int timeslot){
    TIMESLOT_ = timeslot;
}

int timeout_process::setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);

    return old_option;
}

void timeout_process::addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;
    
    if(1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if(one_shot == 1)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//为避免多线程下引起的问题，使用（保存→发送→恢复）原则，避免发生程序错误
void timeout_process::sig_handler(int sig){
   int save_errno = errno;      //防止在多线程环境下其它线程改变了errno标志
   int msg = sig;
   send(pipefd_[1], (char *)&msg, 1, 0);
   errno = save_errno;
}

void timeout_process::addsig(int sig, void (*handler)(int), bool restart){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void timeout_process::timer_handler(){
    timer_list_.tick();
    alarm(TIMESLOT_);       //处理完超时事件后重新开启定时
}

void timeout_process::show_error(int connfd, const char *info){
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

void cb_func(client_data *user_data){
    epoll_ctl(timeout_process::epollfd_, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::epollfd_count_--;    
}
