#ifndef CONFIG_H_
#define CONFIG_H_

#include "WebSever.h"

class config{
    public:
        config();
        ~config() {}
        void parse_arg(int argc, char *argv[]);
    
    public:
        int PORT_;
        int LOGWrite_;              //日志读写方式
        int TRIGMode_;              //触发组合模式
        int LISTENTrigmode_;        //listenfd触发模式
        int CONNTrigmode_;          //connfd触发模式
        int OPT_LINGER_;            //优雅关闭链接
        int sql_num_;               //连接池线程数
        int thread_num_;            //线程池线程数
        int close_log_;             //是否关闭日志
        int actor_model_;           //io并发模式
};
#endif
