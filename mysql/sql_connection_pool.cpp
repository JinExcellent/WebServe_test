#include "sql_connection_pool.h"
#include <cstddef>
#include <cstdlib>
#include <mysql/mysql.h>
#include <unistd.h>

connection_pool::connection_pool(){
    CurConn_ = 0;
    FreeConn_ = 0;
}

//懒汉模式
connection_pool *connection_pool::GetInstance(){
    static connection_pool connPool;
    return &connPool;
}

//构造初始化
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log){
    url_ = url;
    port_ = Port;
    user_ = User;
    password_ = PassWord;
    DatabaseName_ = DBName;
    close_log_ = close_log;

    for(int i = 0; i < MaxConn; i++){
        MYSQL *con = NULL;
        con = mysql_init(con);

        if(con == NULL){
            LOG_ERROR("MYSQL Error.");
            exit(1);
        }  
        con = mysql_real_connect(con, url_.c_str(), user_.c_str(), password_.c_str(), DatabaseName_.c_str(), Port, NULL, 0);
        
        if(con == NULL){
            fprintf(stderr, "Connection failed: %s\n", mysql_error(con));
            LOG_ERROR("MYSQL Error");
            sleep(2);
            exit(1);
        }
        ConnList_.push_back(con);            //数据库连接放入连接池中
        ++FreeConn_;
    }
    
    reserve_ = sem(FreeConn_);
    MaxConn_ = FreeConn_;
}

MYSQL *connection_pool::GetConnection(){
    MYSQL *con = NULL;

    if(0 == ConnList_.size())
        return NULL;

    reserve_.wait();         //这里使用wait是何意？************
    lock_.lock();
    con = ConnList_.front();
    ConnList_.pop_front();

    --FreeConn_;
    ++CurConn_;

    lock_.unlock();
    
    return con;
}

bool connection_pool::ReleaseConnection(MYSQL *con){
    if(NULL == con)
        return false;

    lock_.lock();
    ConnList_.push_back(con);
    ++FreeConn_;
    --CurConn_;
    lock_.unlock();

    reserve_.post();
    return true; 
}

void connection_pool::DestroyPool(){
    lock_.lock();
    if(ConnList_.size() > 0){
        for(auto it = ConnList_.begin(); it != ConnList_.end(); it++){
            MYSQL *con = *it;
            mysql_close(con);
        }
    }
    lock_.unlock();
}

int connection_pool::GetFreeConn(){
    return  this->FreeConn_;
}

connection_pool::~connection_pool(){
    DestroyPool();
}

connectionRALL::connectionRALL(MYSQL **SQL, connection_pool *connPool){
    *SQL = connPool->GetConnection();

    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRALL::~connectionRALL(){
    poolRAII->ReleaseConnection(conRAII);
}
