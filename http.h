#ifndef HTTP_H_
#define HTTP_H_

#include <cstdio>
#include <fcntl.h>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <mysql/mysql.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "./mysql/sql_connection_pool.h"


class http_conn{
    public:
        static const int FILENAME_LEN = 200;
        static const int READ_BUFFER_SIZE = 2048;
        static const int WRITE_BUFFER_SIZE = 1024;
    
        //HTTP请求方法
        enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT};

        /*
         *解析客户端请求,状态机的三种主状态：
         *
         *  CHECK_STATE_REQUESTLINE:解析请求行
         *  CHECK_STATE_HEADER:解析头部字段
         *  CHECK_STATE_CONTENT:解析请求体
         *
         */
        
         enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};

         /*HTTTP_CODE服务器在处理请求时可能会出现的结果：
          *
          *  NO_REQUEST:        :请求数据不完整，需要继续读取客户数据
          *  GET_REQUEST        :获取了一个完整的客户请求数据包
          *  BAD_REQUEST        :客户请求语法错误
          *  NO_RESOURCE        :服务器没有资源
          *  FORBIDDEN_REQUEST  :客户对资源没有足够的访问权限
          *  FILE_REQUEST       :文件请求，文件获取成功
          *  INTERNAL_ERROR     :服务器内部错误
          *  CLOSED_CONNECTION  :客户端已关闭连接
          * */
        
         enum HTTP_CODE{NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION};
        
         /*状态机的三种从状态
          *1.读取到一个完整的行 2.行出错  3.行数据尚且不完整 
          * */

         enum LINE_STATUS {LINE_OK = 0, LINE_BAD, LINE_OPEN};
         
         static int epollfd_;                        //被所有socket上的事件
         static int epollfd_count_;                  //用户数量
         
         MYSQL *mysql_;
         int state_;                                 //读写标志(0读1写)
         
         int improv_;
        /**
        improv 变量被用来标记连接状态是否有改进，主要用于线程池中的任务处理逻辑：

如果连接处理有进展（如成功读到数据或完成写入），improv 会被设置为 1，表示连接的状态已经被成功处理。
在 threadpool 的 run 方法中，improv 会被用来判断是否需要继续处理该任务，或者可以结束当前任务。
在 webserver.cpp 的 dealwithread 和 dealwithwrite 中也会结合 improv 的值来决定后续操作。
        **/
         int timer_flag_;                            //定时器开启标志(1:off,0:on)
    
    private:
        int sockfd_;
        sockaddr_in addr_;
        
        char read_buf_[READ_BUFFER_SIZE];           //读缓冲区
        int read_index_;                            //读指针(指向下一个所要读的指针)
        int checked_index_;                         //当前正在解析的字符在读缓冲区中的下标
        int start_line;                             //当前正在解析的行的起始位置
     
        CHECK_STATE check_state_;                   //主状态机当前所处的状态
        METHOD method_;                             //请求方法
        
        //请求报文的头部信息
        char* url_;
        char* version_;
        char* host_;
        int content_length_;
        bool linger_;
        char real_file_[FILENAME_LEN];
        
        //
        char write_buf_[WRITE_BUFFER_SIZE];
        int write_index_;                           //
        char* file_address_;                        //资源文件被映射到内存中位置
        struct stat file_stat_;                     //描述文件信息的结构体 
        struct iovec iv_[2];                        //使用writev来分离写（响应头部和数据内容分开写）
        int iv_count_;

        int bytes_to_send;                          //将要发送的数据字节数
        int bytes_have_send;                        //已经发送的字节数

        char sql_name_[100];
        char sql_user_[100];
        char sql_passwd[100];


        //新添加的但不知放在何处
        int TRIGMode_;
        int close_log_;
        char *doc_root_;
        int cgi_;            //是否启用POST
        char *string_;       //储存请求头数据

    public:
        void init(int fd, sockaddr_in addr, char *, int , int , std::string user, std::string passwd, std::string sqlname);        //初始化
        void init();                                //请求类中的数据初始化
        void close_conn(bool real_close = true);
        bool read();                                //非阻塞读
        bool write();                               //非阻塞写 
        HTTP_CODE process_read();                   //解析http请求
        bool process_write(HTTP_CODE ret);          //填充http应答
        
        void process();                             //处理客户端请求
        LINE_STATUS parse_line();
        inline char * get_line(){ return read_buf_ + start_line; }
        HTTP_CODE parse_request_line(char * test);
        HTTP_CODE parse_headers(char * test);
        HTTP_CODE parse_content(char * test);
        HTTP_CODE do_request();

        void unmap();
        bool add_response(const char* format, ...);
        bool add_content(const char* content);
        bool add_content_length(int content_length);
        bool add_content_type();
        bool add_status_line(int status, const char* title);
        bool add_headers(int content_length);
        bool add_linger();
        bool add_blank_line();
        
        void initmysql_result(connection_pool *connPool);
        inline sockaddr_in *get_address(){return &addr_;}

};
#endif
