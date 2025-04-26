#ifndef LOCKER_H_
#define LOCKER_H_

#include <pthread.h>
#include <semaphore.h>
#include <exception>
#include <time.h>


//互斥锁
class locker{
    private:
        pthread_mutex_t mutex_;

    public:
        locker(){
            if(pthread_mutex_init(&mutex_, NULL) != 0){
                throw std::exception();
            }
        }

        ~locker(){
            pthread_mutex_destroy(&mutex_);
        }

        bool lock(){
            return pthread_mutex_lock(&mutex_) == 0;
        }
        
        bool unlock(){
            return pthread_mutex_unlock(&mutex_) == 0;
        }
        
        pthread_mutex_t * get(){
            return &mutex_;
        }
};


//条件变量
class cond{
    private:
        pthread_cond_t cond_;

    public:
        cond(){
            if(pthread_cond_init(&cond_, NULL) != 0){
                throw  std::exception();
            }
        }
        ~cond(){
            pthread_cond_destroy(&cond_);
        }

        bool wait(pthread_mutex_t * mutex_){
            int ret = 0;
            ret = pthread_cond_wait(&cond_, mutex_);
            return ret == 0;
        }
        
        //使用条件变量
        bool timewait(pthread_mutex_t *mutex_, struct timespec t){
            int ret = 0;
            ret = pthread_cond_timedwait(&cond_, mutex_, &t);
            return ret == 0;
        }

        bool signal(){
            return pthread_cond_signal(&cond_) == 0;    
        }

        bool broadcast(){
            return pthread_cond_broadcast(&cond_) == 0;
        }
};

//信号量
class sem{
    private:
        sem_t sem_;
    public:
    sem(){
        if(sem_init(&sem_, 0, 0) != 0){
            throw std::exception();
        }
    }

    ~sem(){
        sem_destroy(&sem_);
    }

    bool wait(){
        return sem_wait(&sem_) == 0;
    }
    
    bool post(){
        return sem_post(&sem_) == 0;
    }
};

#endif
