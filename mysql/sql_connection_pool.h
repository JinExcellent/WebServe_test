#ifndef SQL_CONNECTION_POLL_H_
#define SQL_CONNECTION_POLL_H_

#include "../locker.h"
#include "../log/log.h"
#include <list>
#include <mysql/mysql.h>
#include <list>
#include <string>


using std::string;

class connection_pool{
    private:
        connection_pool();
        ~connection_pool();

        int MaxConn_;                           //可接受的最大连接
        int CurConn_;                           //当前已使用连接
        int FreeConn_;                          //池中空闲连接
        locker lock_;                
        std::list<MYSQL *> ConnList_;           //连接池
        sem reserve_;
    public:
        string url_;                            //主机地址
        string port_;                           //数据库端口号
        string user_;                           //登录数据库用户名
        string password_;                       //密码
        string DatabaseName_;                   //数据库名
        int close_log_;                         //是否开启日志
    public:
        MYSQL *GetConnection();                 //获取数据库连接
        bool ReleaseConnection(MYSQL *conn);    //释放连接
        int GetFreeConn();                      //获取空闲连接
        void DestroyPool();                     //销毁池
        
        static connection_pool *GetInstance();  //单例模式

        void init(string url, string user, string password, string databasename, int port, int maxconn, int close_log);
};

//使用RALL机制来使用数据库
class connectionRALL{
    public:
        connectionRALL(MYSQL **conn, connection_pool *connPool);
        ~connectionRALL();
    private:
        MYSQL *conRAII;
        connection_pool *poolRAII;

};

#endif
