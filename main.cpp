#include "config.h"

int main(int argc, char *argv[]){
    //初始化默认数据库信息
    string user = "root";
    string passwd = "root";
    string databasename = "mainDB";

    config configration;
    configration.parse_arg(argc, argv);

    WebServer server;

    server.init(configration.PORT_, user, passwd, databasename, configration.LOGWrite_, configration.OPT_LINGER_, configration.sql_num_, configration.TRIGMode_, configration.thread_num_, configration.close_log_, configration.actor_model_);

    server.log_write();
    server.sql_pool();
    server.thread_pool();
    server.trig_mode();         //服务器触发模式
    server.eventListen();       //开启监听
    server.eventLoop();         //运行服务器

    return 0;
}
