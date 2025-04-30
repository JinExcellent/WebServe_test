#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <cstddef>
#include <pthread.h>
#include <exception>
#include <cstdio>
#include <list>
#include "locker.h"
#include "http.h"
#include "mysql/sql_connection_pool.h"


template <typename T>
class threadpool{
    private:
        int thread_number_;             //工作线程数量
        pthread_t * threads_;           //预分配的线程池指针
        int max_requests_;              //支持的最大请求数量
        locker queue_locker_;
        sem queue_status_;      
        bool thread_stop_;              //发出信号，显示线程池是否停止
        std::list<T *> work_queue_;     //请求队列
        connection_pool *connpool_;      //数据库连接池
        int actor_model_;               //模型切换

    private:
        static void* worker(void* arg);     //创建线程的工作函数
        void run();

    public:
        threadpool(int actor_model, connection_pool *connpool, int thread_number = 8, int max_requests = 10000);
        ~threadpool();
        bool append(T * request);
        bool append(T * request, int state);
};

template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connpool, int thread_number, int max_requests): actor_model_(actor_model), thread_number_(thread_number), max_requests_(max_requests), thread_stop_(false), threads_(NULL), connpool_(connpool){
        if((thread_number <= 0) || (max_requests <= 0)){
            throw std::exception();
        }

        threads_ = new pthread_t[thread_number];
        if(!threads_){
            throw std::exception();
        }

        for(int i = 0; i < thread_number; i++){
            printf("created the %dth thread.\n", i + 1);
            if(pthread_create(threads_ + i, NULL, worker, this) != 0){
                delete [] threads_;         //对于异常的处理比较强硬，是否有更好的方法处理呢？
                throw std::exception();
            }

            if(pthread_detach(threads_[i])){
                delete [] threads_;
                throw std::exception();
            }
        }
}

template<typename T>
threadpool<T>::~threadpool(){
    delete [] threads_;
    thread_stop_ = true;
}

//Reactor
template<typename T>
bool threadpool<T>::append(T *request, int state){
    queue_locker_.lock();
    if(work_queue_.size() >= max_requests_){
        queue_locker_.unlock();
        return false;
    }
    request->state_ = state;
    work_queue_.push_back(request);
    queue_locker_.unlock();
    queue_status_.post();

    return true;
}

//Proactor
template<typename T>
bool threadpool<T>::append(T *request){
    queue_locker_.lock();
    if(work_queue_.size() >= max_requests_){
        queue_locker_.unlock();
        return false;
    }

    work_queue_.push_back(request);
    queue_locker_.unlock();
    queue_status_.post();   //保证在线程在取请求的同步关系
    
    return true;
}

//要清楚这里的worker函数为什么是静态类型的函数
template<typename T>
void* threadpool<T>::worker(void * arg){
    threadpool * pool = (threadpool *)arg;
    pool -> run();
    return pool;
}

template<typename T>
void threadpool<T>::run(){
    while(!thread_stop_){
        queue_status_.wait();
        queue_locker_.lock();
        
        if(work_queue_.empty()){
            queue_locker_.unlock();
            continue;
        }

        T* request = work_queue_.front();
        work_queue_.pop_front();
        queue_locker_.unlock();
        
        if(!request)
            continue;
        //根据actor_model选用不同的io复用模型
        if(actor_model_ == 1){
            //state_用于区分读写操作
            if(request->state_ == 0){
                if(request->read()){
                    request->improv_ = 1;
                    //创建数据库实例并初始化
                    connectionRALL mysqlcon(&request->mysql_, connpool_);
                    request->process();
                }else{
                    request->improv_ = 1;
                    request->timer_flag_ = 1;
                }
            }else{
                if(request->write()){
                    request->improv_ = 1;
                }else{
                    request->improv_ = 1;
                    request->timer_falg_ = 1;
                }
            }
        }else{
            connectionRALL mysqlcon(&request->mysql_, connpool_);
            request->process();
        }
    }
}
#endif
