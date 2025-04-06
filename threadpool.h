#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <cstddef>
#include <pthread.h>
#include <exception>
#include <cstdio>
#include <list>
#include "locker.h"


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
    
    private:
        static void* worker(void* arg);     //创建线程的工作函数
        void run();

    public:
        threadpool(int thread_number = 8, int max_requests = 10000);
        ~threadpool();
        bool append(T * request);
};

template <typename T>
threadpool<T>::threadpool(int thread_number, int max_requests):
    thread_number_(thread_number), max_requests_(max_requests),
    thread_stop_(false), threads_(NULL){
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

template<typename T>
bool threadpool<T>::append(T *request){
    queue_locker_.lock();
    if(work_queue_.size() >= max_requests_){
        queue_locker_.unlock();
        return false;
    }

    work_queue_.push_back(request);
    work_queue_.unlock();
    queue_status_.post();   //保证在线程在取请求的同步关系
    return true;
}

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
        
        if(!request){
            continue;
        }
        request -> process();
    }

}


































#endif
