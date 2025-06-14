#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class LockedQueue {
private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable condition_;
    size_t max_size_;
    
public:
    explicit LockedQueue(size_t max_size = 1024) : max_size_(max_size) {}
    
    // 禁止拷贝和移动
    LockedQueue(const LockedQueue&) = delete;
    LockedQueue& operator=(const LockedQueue&) = delete;
    LockedQueue(LockedQueue&&) = delete;
    LockedQueue& operator=(LockedQueue&&) = delete;
    
    // 入队操作
    template<typename U>
    bool enqueue(U&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (queue_.size() >= max_size_) {
            return false;  // 队列已满
        }
        
        queue_.push(std::forward<U>(item));
        condition_.notify_one();
        return true;
    }
    
    // 出队操作（非阻塞）
    bool dequeue(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (queue_.empty()) {
            return false;  // 队列为空
        }
        
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    // 阻塞式出队操作
    void dequeue_blocking(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        while (queue_.empty()) {
            condition_.wait(lock);
        }
        
        item = std::move(queue_.front());
        queue_.pop();
    }
    
    // 检查队列是否为空
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    // 检查队列是否已满
    bool full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size() >= max_size_;
    }
    
    // 获取当前队列大小
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    // 获取队列容量
    size_t capacity() const {
        return max_size_;
    }
}; 