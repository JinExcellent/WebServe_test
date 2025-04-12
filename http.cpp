#include "http.h"
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <strings.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/uio.h>

// 定义HTTP响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

const char * doc_root = "/home/jin/WebServe_test/resource";          //服务器文件目录

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
   init();
}

void http_conn::init(){
    bytes_to_send = 0;
    bytes_have_send = 0;

    check_state_ = CHECK_STATE_REQUESTLINE;
    linger_ = false;
    
    method_ = GET;
    url_ = 0;
    version_ = 0;
    content_length_ = 0;
    host_ = 0;
    start_line = 0;
    checked_index_ = 0;
    read_index_ = 0;
    write_index_ = 0;

    memset(read_buf_, 0, READ_BUFFER_SIZE);
    memset(write_buf_, 0, WRITE_BUFFER_SIZE);
    memset(real_file_, 0, FILENAME_LEN);
}

void http_conn::close_conn(){
    if(sockfd_ != -1){
        removefd(epollfd_, sockfd_);
        sockfd_ = -1;
        epollfd_count_--;
    }
}

//由池中的工作线程调用，该函数是处理http请求的入口地址
void http_conn::process(){
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST){
        modfd(epollfd_, sockfd_, EPOLLIN);
        return;
    }

    bool write_ret = process_write(read_ret);
    if(!write_ret){
        close_conn();
    }
    modfd(epollfd_, sockfd_, EPOLLOUT);
}

http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;

    for(; checked_index_ < read_index_; checked_index_++){
        temp = read_buf_[checked_index_];
        if(temp == '\r'){
            //不完整的读取
            if((checked_index_ + 1) == read_index_){        //***********************************
                return LINE_OPEN;
            //完整的读取
            }else if(read_buf_[checked_index_ + 1] == '\n'){
                read_buf_[checked_index_++] = '\0';
                read_buf_[checked_index_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }else if(temp == '\n'){
            if((checked_index_ > 1) && (read_buf_[checked_index_ - 1] == '\r')){
                read_buf_[checked_index_-1] = '\0';
                read_buf_[checked_index_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

http_conn::HTTP_CODE http_conn::parse_headers(char *text){
    if(text[0] == '\0'){
        if(content_length_ != 0){
            check_state_ = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }else if(strncasecmp(text, "Connection:", 11) == 0){
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0){
            linger_ = true;
        }
    }else if(strncasecmp(text, "Content-Length:", 15) == 0){
        text += 15;
        text += strspn(text, " \t");
        content_length_ = atol(text);
    }else if(strncasecmp(text, "Host:", 5) == 0){
        text += 5;
        text += strspn(text, " \t");
        host_ = text;
    }else{
        printf("oops, unknow header %s\n", text);
    }

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text){
    if(read_index_ >= (content_length_ + checked_index_)){
        text[content_length_] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST; 
} 

http_conn::HTTP_CODE http_conn::parse_request_line(char *text){ 
    url_ = strpbrk(text, " \t"); 
    if(!url_){ 
        printf("url_ error\n"); 
        return BAD_REQUEST;
    }

    *url_++ = '\0';     //(*url_)++ = '\0' 分清这两个语句之间的语序
    char* method = text;
    if(strcasecmp(method, "GET")== 0){
        method_ = GET;
    }else {
        printf("method_ error\n");
        return BAD_REQUEST;
    }

    version_ = strpbrk(url_, " \t");
    if(!version_){ 
        printf("version_ error_line\n");
        return BAD_REQUEST;
    }
    *version_++ = '\0';
    if(strcasecmp(version_, "HTTP/1.1") != 0){
        printf("version_error_line\n");
        return BAD_REQUEST;
    }

    if(strncasecmp(url_, "http://", 7) == 0){ //**********************************
        url_ += 7;
        url_ = strchr(url_, '/');
    }
    if(!url_ || url_[0] != '/'){
        printf("resource error\n");
        return BAD_REQUEST;
    }
    check_state_ = CHECK_STATE_HEADER;      //处理完头部后检查请求状态
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read(){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    while(((check_state_ == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) || ((line_status = parse_line()) == LINE_OK)){
        text = get_line();
        start_line = checked_index_;        //****************************
        printf("got 1 http line: %s\n", text);

        switch (check_state_) {
            case CHECK_STATE_REQUESTLINE: {
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST){
                    printf("process requestline error\n");
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:{
                ret = parse_headers(text);
                if(ret == BAD_REQUEST){
                    printf("process header error\n");
                    return BAD_REQUEST;
                }
                else if(ret == GET_REQUEST)
                    return do_request();
                break;
            } 
            case CHECK_STATE_CONTENT:{
                ret = parse_content(text);
                if(ret == GET_REQUEST){
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            default: {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request(){
    strcpy(real_file_, doc_root);
    int len = strlen(doc_root);
    strncpy(real_file_ + len, url_, FILENAME_LEN - len - 1);

    if(stat(real_file_, &file_stat_) < 0){
        return NO_RESOURCE;
    }
    
    if(!(file_stat_.st_mode & S_IROTH)){
        return FORBIDDEN_REQUEST;
    }

    if(S_ISDIR(file_stat_.st_mode)){
        return BAD_REQUEST;
    }       

    int fd = open(real_file_, O_RDONLY);
    //创建内存映射
    file_address_ = (char*)mmap(0, file_stat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0); 
    close(fd);
    return FILE_REQUEST;
    
}

void http_conn::unmap(){
    if(file_address_){
        munmap(file_address_, file_stat_.st_size);
        file_address_ = 0;
    }
}

bool http_conn::process_write(HTTP_CODE ret){
    switch (ret) {
        case INTERNAL_ERROR:
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form)){
                return false;
            }
            break;
        case BAD_REQUEST:
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if(!add_content(error_400_form)){
                return false;
            }
            break;
        case NO_RESOURCE: 
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form)){
                return false;
            }
            break;
        case FORBIDDEN_REQUEST: 
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form)){
                return false;
            }
            break;
        case FILE_REQUEST:
            add_status_line(200, ok_200_title);
            add_headers(file_stat_.st_size);
            iv_[0].iov_base = write_buf_;
            iv_[0].iov_len = write_index_;
            iv_[1].iov_base = file_address_;
            iv_[1].iov_len = file_stat_.st_size;
            iv_count_ = 2;

            bytes_to_send = write_index_ + file_stat_.st_size;

            return true;
        default:
            return false;
    }
    iv_[0].iov_base = write_buf_;
    iv_[0].iov_len = write_index_;
    iv_count_ = 1;
    bytes_to_send = write_index_;
    return true;
}

bool http_conn::add_response(const char *format, ...){
    if(write_index_ >= WRITE_BUFFER_SIZE)
        return false;

    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(write_buf_ + write_index_, WRITE_BUFFER_SIZE - 1 - write_index_, format, arg_list);
    if(len >= WRITE_BUFFER_SIZE - 1 - write_index_)
        return false;

    write_index_ += len;
    va_end(arg_list);

    return true;
}

bool http_conn::add_status_line(int status, const char *title){
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title); 
}

bool http_conn::add_headers(int content_len){
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length(int content_len){
    return add_response("Content-Length: %d\r\n", content_len);
}

bool http_conn::add_linger(){
    return add_response("Connection: %s\r\n", (linger_ == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line(){
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char *content){
    return add_response("%s", content);
}

bool http_conn::add_content_type(){
    return add_response("Content-Type:%s\r\n", "text/html");
}
    

bool http_conn::read(){
   if(read_index_ >= READ_BUFFER_SIZE){
        return false;
   }
   int bytes_read = 0;
   while(true){
        bytes_read = recv(sockfd_, read_buf_ + read_index_, READ_BUFFER_SIZE - read_index_, 0);
        if(bytes_read == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK) {   //****************
                //无数据
                break;
            }
            return false;
        }else if(bytes_read == 0)   //对端关闭连接
            return false;
        read_index_ += bytes_read;
   }
   printf("read:\n %s \n", read_buf_);
   return  true;
}

bool http_conn::write(){
   int temp = 0;

   if(bytes_to_send == 0){
        modfd(epollfd_, sockfd_, EPOLLIN);
        init();
        return true;
   }

   while(1){
        temp = writev(sockfd_, iv_, iv_count_);
        if(temp <= -1){
            if(errno == EAGAIN){
                modfd(epollfd_, sockfd_, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;

        if(bytes_have_send >= iv_[0].iov_len){
            iv_[0].iov_len = 0;
            iv_[1].iov_base = file_address_ + (bytes_have_send - write_index_);
            iv_[1].iov_len = bytes_to_send;
        }else{
            iv_[0].iov_base = write_buf_ + bytes_have_send;
            iv_[0].iov_len = iv_[0].iov_len - temp;
        }

        if(bytes_to_send <= 0){
            unmap();
            modfd(epollfd_, sockfd_, EPOLLIN);
        
            if(linger_){
                init();
                return true;
            }else
                return false;
        }
   }
}













