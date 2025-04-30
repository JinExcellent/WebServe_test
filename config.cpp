#include "config.h"
#include <bits/getopt_core.h>
#include <cstdlib>

config::config(){
    PORT_ = 9006;
    LOGWrite_ = 0;              //默认同步
    TRIGMode_ = 0;              //默认 listenfd LT，connfd LT
    LISTENTrigmode_ = 0;        //默认 LT
    CONNTrigmode_ = 0;          //默认 LT    
    OPT_LINGER_ = 0;            //默认不使用
    sql_num_ = 8;               
    thread_num_ = 8;
    close_log_ = 0;             //默认不关闭
    actor_model_ = 0;           //默认proactor
}

void config::parse_arg(int argc, char *argv[]){
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";

    while((opt = getopt(argc, argv, str))){
        switch (opt) {
            case 'p':
                PORT_ = atoi(optarg);
                break;
            case 'l':
                LOGWrite_ = atoi(optarg);
                break;
            case 'm':
                TRIGMode_ = atoi(optarg);
                break;
            case 'o':
                OPT_LINGER_ = atoi(optarg);
                break;
            case 's':
                sql_num_ = atoi(optarg);
                break;
            case 't':
                thread_num_ = atoi(optarg);
                break;
            case 'c':
                close_log_ = atoi(optarg);
                break;
            case 'a':
                actor_model_ = atoi(optarg);
            default:
                break;
        }
    }

}
