#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>
#include <iostream>
#include <string>
#include "block_queue.h"
#include "../locker.h"


class log{
    private:
        char dir_name[128];
        char log_name[128];
        int split_lines_;
        int log_buf_size_;
        long long count_;
        int today_;
        FILE* fp_;                                       //日志文件描述符
        char* buf_;
        block_queue<std::string> *log_queue_;            //阻塞队列
        bool is_async_;                                  //异步标志位
        locker mutex_;
        int close_log_;

    private:
        log();
        virtual ~log();                                 //这里使用虚函数的用意？*********
        inline void *async_write_log(){                 //将一条日志记录从阻塞队列中拿出，写到日志文件中
            std::string single_log;

            while (log_queue_ -> pop(single_log)) {
                mutex_.lock();
                fputs(single_log.c_str(), fp_);
                mutex_.unlock();
            }
        }

    public:
        static log *get_instance(){                     //使用懒汉式的单例模式
            static log instance;
            return &instance;
        }

        static void *flush_log_thread(void *args){
            log::get_instance() -> async_write_log();
        }

        bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 50000, int max_queue_size = 0);
        void write_log(int level, const char *format, ...);
        void flush();
};

#define LOG_DEBUG(format, ...) if(0 == close_log_){log::get_instance() -> write_log(0, format, ##__VA_ARGS__);log::get_instance -> flush();}

#define LOG_INFO(format, ...) if(0 == close_log_){log::get_instance() -> write_log(1, format, ##__VA_ARGS__);log::get_instance -> flush();}

#define LOG_WARN(format, ...) if(0 == close_log_){log::get_instance() -> write_log(2, format, ##__VA_ARGS__);log::get_instance -> flush();}

#define LOG_ERROR(format, ...) if(0 == close_log_){log::get_instance() -> write_log(3, format, ##__VA_ARGS__);log::get_instance -> flush();}

#endif
