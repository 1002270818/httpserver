/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft Apache 2.0
 */ 

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>

struct PoolConfig {
    std::mutex mtx;     //互斥量
    std::condition_variable cond;   // 条件变量
    bool isClosed;      // 是否关闭
    std::queue<std::function<void()>> tasks; // 队列(保存的是任务)
};

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<PoolConfig>()) {
        for(size_t i = 0; i < threadCount; i++) {
            std::thread([pool = pool_] {
                std::unique_lock<std::mutex> locker(pool->mtx);        //线程创建时尝试获取锁
                while(true) {
                    if(!pool->tasks.empty()) {
                        std::function<void()> task = std::move(pool->tasks.front());
                        pool->tasks.pop();                             //有任务则获取任务后解锁
                        locker.unlock();
                        task();
                        locker.lock();                                 //完成了接着尝试获取锁
                    }
                    else if(pool->isClosed) break;
                    else pool->cond.wait(locker);                      //无任务时阻塞等待条件唤醒
                }
            }).detach();
        }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {
        if(static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);             //lock_guard
                pool_->isClosed = true;
            }
            pool_->cond.notify_all();
        }
    }

    template<class F>
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));
        }
        pool_->cond.notify_one();
    }

private:
    // 池子结构体
    std::shared_ptr<PoolConfig> pool_;
};

#endif //THREADPOOL_H