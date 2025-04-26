#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H


#include "../locker.h"
#include <sys/time.h>
#include <cstddef>
#include <cstdlib>
#include <utility>


template<typename T>
class block_queue{
    private:
        locker mutex_;
        cond cond_;
        
        T* array_;
        int size_;
        int max_size_;
        int front_;         //队头
        int back_;          //队尾
        
    public:
        block_queue(int max_size = 1000){
            if(max_size < 0){
                exit(-1);
            }

            max_size_ = max_size;
            array_ = new T[max_size];
            size_ = 0;
            front_ = -1;
            back_ = -1;
        }

        inline void clear(){
            mutex_.lock();
            size_ = 0;
            front_ = -1;
            back_ = -1;
            mutex_.unlock();
        }

        ~block_queue(){
            mutex_.lock();
            if(array_ != NULL){
                delete [] array_;
            }
            mutex_.unlock();
        }

        inline bool full(){
            mutex_.lock();
            if(size_ >= max_size_){
                mutex_.unlock();
                return true;
            }
            mutex_.unlock();
            return  false;
        }

        inline bool empty(){    
            mutex_.lock();
            if(size_ == 0){
                mutex_.unlock();
                return true;
            }
            mutex_.unlock();
            return false;
        }

        inline bool front(T &value){
            mutex_.lock();
            if(size_ == 0){
                mutex_.unlock();
                return false;
            }
            value = array_[front_];
            mutex_.unlock();
            return true;
        }
        
        inline bool back(T &value){
            mutex_.lock();
            if(size_ == 0){
                mutex_.unlock();
                return false;
            }
            value = array_[back_];
            mutex_.unlock();
            return true;
        }

        inline int size(){
            int temp = 0;
            mutex_.lock();
            temp = size_;
            mutex_.unlock();

            return temp;
        }

        inline int max_size(){
            int temp = 0;
            mutex_.lock();
            temp = max_size_;
            mutex_.unlock();

            return temp;
        }

        bool push(const T &item){
            mutex_.lock();
            if(size_ >= max_size_){
                cond_.broadcast();
                mutex_.unlock();
                return false;
            }

            back_ = (back_ + 1) % max_size_;        //采用循环队列
            array_[back_] = item;

            size_++;
            cond_.broadcast();
            mutex_.unlock();

            return true;
        }

        bool pop(T &item){
            mutex_.lock();
            while(size_ <= 0){
                if(!cond_.wait(mutex_.get())){
                    mutex_.unlock();
                    return false;
                }
            }
            
            front_ = (front_ + 1) % max_size_;
            item = array_[front_];
            size_--;
            mutex_.unlock();
            
            return true;
        }
        
        //增加超时处理的pop重载
        bool pop(T &item, int timeout){
            struct timespec t = {0,0};      //接受绝对时间
            struct timeval now = {0,0};     //接受当前时间
        
            gettimeofday(&now, NULL);
            mutex_.lock();
            if(size_ <= 0){
                //计算绝对超时时间保存在t结构体中
                //这里的timeout保存的是毫秒部分，需要转换为秒
                t.tv_sec = now.tv_sec + timeout / 1000;
                //将timeout中剩余的毫秒转化为纳秒
                t.tv_nsec = (timeout % 1000) * 1000;        //这里是否有错误？ 转换为纳秒应该是*1 000 000
                
                if(!cond_.timewait(mutex_.get(), t)){
                    mutex_.unlock();
                    return false;
                }
            }

            //第二次检测用于防止虚假唤醒（这书虚假唤醒处理的第二种方式）
            if(size_ <= 0){
                mutex_.unlock();
                return false;
            }

            front_ = (front_ + 1) % max_size_;
            item = array_[front_];
            size_++;
            mutex_.unlock();

            return true;
        }
};
#endif

