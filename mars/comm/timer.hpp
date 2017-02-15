//
//  timer.h
//  comm
//
//  Created by lion xing on 9/2/2017.
//  Copyright © 2017 Tencent. All rights reserved.
//

#ifndef timer_h
#define timer_h

#include "boost/thread.hpp"
#include "boost/function.hpp"
#include "boost/utility/result_of.hpp"


class Timer{

public:
    Timer() :expired_(true), try_to_expire_(false){
    }
    
    Timer(const Timer& t){
        expired_ = t.expired_.load();
        try_to_expire_ = t.try_to_expire_.load();
    }
    
    ~Timer(){
        Expire();
    }
    
    void StartTimer(int interval, boost::function<void()> task){
        if (expired_ == false){
            return;
        }
        expired_ = false;
        boost::thread([this, interval, task](){
            while (!try_to_expire_){
                boost::this_thread::sleep_for(boost::chrono::milliseconds(interval));
                task();
            }
            {
                boost::lock_guard<boost::mutex> locker(mutex_);
                expired_ = true;
                expired_cond_.notify_one();
            }
        }).detach();
    }
    
    void StopTimer(){
        Expire();
    }

    void Expire(){
        if (expired_){
            return;
        }
        
        if (try_to_expire_){
            return;
        }
        try_to_expire_ = true;
        {
            boost::unique_lock<boost::mutex> locker(mutex_);
            expired_cond_.wait(locker, [this]{return expired_ == true; });
            if (expired_ == true){
                try_to_expire_ = false;
            }
        }
    }
    
    template<typename callable, class... arguments>
    void SyncWait(int after, callable&& f, arguments&&... args){
        
        boost::function<typename boost::result_of<callable(arguments...)>::type()> task
        (boost::bind(boost::forward<callable>(f), boost::forward<arguments>(args)...));
        boost::this_thread::sleep_for(boost::chrono::milliseconds(after));
        task();
    }
    template<typename callable, class... arguments>
    void AsyncWait(int after, callable&& f, arguments&&... args){
        boost::function<typename boost::result_of<callable(arguments...)>::type()> task
        (boost::bind(boost::forward<callable>(f), boost::forward<arguments>(args)...));
        
        boost::thread([after, task](){
            boost::this_thread::sleep_for(boost::chrono::milliseconds(after));
            task();
        }).detach();
    }
    
private:
    boost::atomic<bool> expired_;
    boost::atomic<bool> try_to_expire_;
    boost::mutex mutex_;
    boost::condition_variable expired_cond_;
};

#endif /* timer_h */
