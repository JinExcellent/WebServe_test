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
        int thread_number_;
        pthread_t * threads_;
        int max_requests_;
        locker queue_locker_;
        sem queue_status_;
        bool thread_stop_;
        std::list<T *> work_queue_;
    
    private:
        static void* worker(void* arg);
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
                delete [] threads_;
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
    work_queue_.unique();
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
