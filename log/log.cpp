#include "log.h"
#include "block_queue.h"
#include <bits/types/struct_timeval.h>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <pthread.h>
#include <string.h>
#include <sys/select.h>


log::log(){
    count_ = 0;
    is_async_ = 0;
}

log::~log(){
    if(fp_ != NULL){
        fclose(fp_);
    }
}

//不设置队列大小为同步模式，否则非异步模式
bool log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size){
    //如果设置了队列大小，则为异步写模式
    if(max_queue_size >= 1){
        is_async_ = true;
        log_queue_ = new block_queue<std::string>(max_queue_size);
        pthread_t tid;

        //创建子线程用于异步写日志
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }

    close_log_ = close_log;
    log_buf_size_ = log_buf_size;
    buf_ = new char[log_buf_size];
    memset(buf_, 0 , log_buf_size_);
    split_lines_ = split_lines;
    
    //**************************************************
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};
    
    //如果没有找到/，自定义文件名（使用日期加文件名）
    if(p == NULL){
        //这里的tm_year保存到的是年份的偏移量，故需要加1900（这个结构中的变量是从1900开始计算年份），月份加一是因为这个变量是从零开始计数
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }else{
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);            //将目录路径写入dir_name变量中
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s",dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    today_ = my_tm.tm_mday;

    fp_ = fopen("log_full_name", "a");
    if(fp_ == NULL){
        return false;
    }

    return true;
}

void log::write_log(int level, const char *format, ...){
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};

    switch (level) {
        case 0:
            strcpy(s, "[debug]");
            break;
        case 1:
            strcpy(s, "[info]");
            break;
        case 2:
            strcpy(s, "[warn]");
            break;
        case 3:
            strcpy(s, "[erro]");
            break;
        default:
            strcpy(s, "[info]");
            break;
    }
    
    mutex_.lock();
    count_++;           //在插入日志前++，主要是为了检测单个日志文件是否达到最大行数限制
    
    if(today_ != my_tm.tm_mday || count_ % split_lines_ == 0){
        char new_log[256] = {0};
        fflush(fp_);        //在关闭描述符前，将其中所有的数据立即写入到对应的文件中
        fclose(fp_);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
        
        //如果是新的一天，则创建一个新的日志文件
        if(today_ != my_tm.tm_mday){
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            today_ = my_tm.tm_mday;
            count_ = 0;
        }else {
        //如果是超出了单个文件规定的行数，则创建一个新文件并带上文件分割后缀 （count_ / split_lines_）
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, count_ / split_lines_);   
        }
        fp_ = fopen(new_log, "a");
    }

    mutex_.unlock();

    va_list valst;
    va_start(valst, format);
    
    std::string log_str;
    mutex_.lock();
    int n = snprintf(buf_, 48, "%d-02%d-02%d %02d:%02d:%02d.%06ld %s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    int m = vsnprintf(buf_ + n, log_buf_size_ - n - 1, format, valst);
    buf_[n + m] = '\n';
    buf_[n + m + 1] = '\0';
    log_str = buf_;

    mutex_.unlock();    //思考：为何在这里解锁？ 如果一个线程得到log_str后没有及时放入日志队列中，是否会造成其它线程在执行该函数时会覆盖掉上一个线程赋值的log_str?
    if(is_async_ && !log_queue_ -> full()){
        log_queue_ -> push(log_str);
    }else {
        //否则执行同步操作
        mutex_.lock();
        fputs(log_str.c_str(), fp_);
        mutex_.unlock();
    }
    va_end(valst);
}

void log::flush(){
    mutex_.lock();
    fflush(fp_);
    mutex_.unlock();
}
