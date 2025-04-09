#include "threadpool.h"
#include "http.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <list>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>


#define MAX_FD 65535
#define MAX_EVENTS 10000

extern void addfd(int , int , bool );
extern void removfd(int , int);

int main(int argc, char * argv[]){
    
    if(argc < 2){
        printf("Usage: %s <port>\n", argv[0]);
        exit(-1);
    }

       threadpool<http_conn> * pool = NULL;
    try{
        pool = new threadpool<http_conn>;
    }catch(...){
        return 1;
    }

    http_conn* users = new http_conn[MAX_FD];   //
    
    struct sockaddr_in serv_addr, client_addr;
    socklen_t client_addr_sz;

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //设置地址和端口q复用
    int reuse = true;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse, sizeof(reuse));
    if(bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1){
        perror("bind error.");
        exit(-1);
    }
    
    if(listen(listenfd, 5) == -1){
        perror("bind error.");
        exit(-1);
    }
    
    //设置epoll标志
    int epollfd = epoll_create(MAX_EVENTS);
    epoll_event* events = new epoll_event[MAX_EVENTS];
    
    addfd(epollfd, listenfd, false);
    http_conn::epollfd_ = epollfd;
    
    while (true) {
        int ready_num = epoll_wait(epollfd, events, MAX_EVENTS, -1);

        //EINTR：read manual
        if((ready_num < 0) && (errno != EINTR)){
            perror("epoll_wait error.");
            break;
        }
    
        for(int i = 0; i < ready_num; i++){
            int sockfd = events[i].data.fd;

            if(sockfd == listenfd){
                client_addr_sz = sizeof(client_addr);
                int connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_addr_sz);
                
                if(connfd < 0){
                    printf("errno is: %d\n", errno);
                    continue;
                }

                if(http_conn::epollfd_count_ >= MAX_FD){
                    printf("to many request, pls wait.\n");
                    continue;
                }
                users[connfd].init(connfd, client_addr);
                //满足多个标志中的一个，即成立
            }else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                users[sockfd].close_conn();
            }else if(events[i].events & EPOLLIN){
                if(users[sockfd].read())
                    pool->append(users + sockfd);       //这里直接使用创建的文件描述符作为数组下标
                                                        //要注意的是，linux系统中用户可用的第一个描述符为3
                else
                    users[sockfd].close_conn();

            }else if(events[i].events & EPOLLOUT){
                if(!users[sockfd].write()){
                    users[sockfd].close_conn();
                }
            }
        }
    }

    close(epollfd);
    close(listenfd);
    delete [] users;
    delete [] pool;
    delete [] events;

    return 0;
}






























