#pragma once

#include <atomic>
#include <memory>
#include <type_traits>

template<typename T, size_t Size>
class SPSCLockFreeQueue {
    static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");
    
private:
    struct alignas(64) HeadData {  // 避免false sharing
        std::atomic<size_t> head;
    } head_data_;
    
    struct alignas(64) TailData {  // 避免false sharing
        std::atomic<size_t> tail;
    } tail_data_;
    
    struct alignas(64) BufferData {
        T buffer[Size];
    } buffer_data_;
    
    static constexpr size_t MASK = Size - 1;
    
public:
    SPSCLockFreeQueue() 
        : head_data_{0}, tail_data_{0} { // 初始化具名结构体成员
    }
    ~SPSCLockFreeQueue() = default;
    
    // 禁止拷贝和移动
    SPSCLockFreeQueue(const SPSCLockFreeQueue&) = delete;
    SPSCLockFreeQueue& operator=(const SPSCLockFreeQueue&) = delete;
    SPSCLockFreeQueue(SPSCLockFreeQueue&&) = delete;
    SPSCLockFreeQueue& operator=(SPSCLockFreeQueue&&) = delete;
    
    // 生产者端：入队操作
    template<typename U>
    bool enqueue(U&& item) {
        const size_t current_tail = tail_data_.tail.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & MASK;
        
        // 检查队列是否已满
        if (next_tail == head_data_.head.load(std::memory_order_acquire)) {
            return false;  // 队列已满
        }
        
        // 存储数据
        buffer_data_.buffer[current_tail] = std::forward<U>(item);
        
        // 更新tail指针
        tail_data_.tail.store(next_tail, std::memory_order_release);
        return true;
    }
    
    // 消费者端：出队操作
    bool dequeue(T& item) {
        const size_t current_head = head_data_.head.load(std::memory_order_relaxed);
        
        // 检查队列是否为空
        if (current_head == tail_data_.tail.load(std::memory_order_acquire)) {
            return false;  // 队列为空
        }
        
        // 读取数据
        item = std::move(buffer_data_.buffer[current_head]);
        
        // 更新head指针
        head_data_.head.store((current_head + 1) & MASK, std::memory_order_release);
        return true;
    }
    
    // 检查队列是否为空
    bool empty() const {
        return head_data_.head.load(std::memory_order_acquire) == tail_data_.tail.load(std::memory_order_acquire);
    }
    
    // 检查队列是否已满
    bool full() const {
        const size_t current_tail = tail_data_.tail.load(std::memory_order_acquire);
        const size_t next_tail = (current_tail + 1) & MASK;
        return next_tail == head_data_.head.load(std::memory_order_acquire);
    }
    
    // 获取当前队列大小（近似值）
    size_t size() const {
        const size_t current_head = head_data_.head.load(std::memory_order_acquire);
        const size_t current_tail = tail_data_.tail.load(std::memory_order_acquire);
        return (current_tail - current_head) & MASK;
    }
    
    // 获取队列容量
    static constexpr size_t capacity() {
        return Size - 1;  // 实际可用容量比Size小1
    }
};