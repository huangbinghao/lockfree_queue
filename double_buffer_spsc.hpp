#pragma once

#include <atomic>
#include <vector>
#include <memory>

template<typename T>
class DoubleBufferSPSC {
private:
    std::vector<T> buffer1_;
    std::vector<T> buffer2_;
    std::atomic<std::vector<T>*> write_buffer_;
    std::atomic<std::vector<T>*> read_buffer_;
    std::atomic<bool> buffer_swapped_{false};
    std::atomic<size_t> write_index_{0};
    std::atomic<size_t> read_index_{0};
    size_t max_size_;
    
public:
    explicit DoubleBufferSPSC(size_t max_size = 1024) 
        : max_size_(max_size) {
        buffer1_.reserve(max_size);
        buffer2_.reserve(max_size);
        write_buffer_.store(&buffer1_);
        read_buffer_.store(&buffer2_);
    }
    
    // 禁止拷贝和移动
    DoubleBufferSPSC(const DoubleBufferSPSC&) = delete;
    DoubleBufferSPSC& operator=(const DoubleBufferSPSC&) = delete;
    DoubleBufferSPSC(DoubleBufferSPSC&&) = delete;
    DoubleBufferSPSC& operator=(DoubleBufferSPSC&&) = delete;
    
    // 生产者端：写入数据
    template<typename U>
    bool enqueue(U&& item) {
        auto* current_write_buffer = write_buffer_.load(std::memory_order_acquire);
        
        if (current_write_buffer->size() >= max_size_) {
            return false;  // 写缓冲区已满
        }
        
        current_write_buffer->push_back(std::forward<U>(item));
        return true;
    }
    
    // 生产者端：切换缓冲区
    void swap_buffers() {
        auto* current_write = write_buffer_.load(std::memory_order_acquire);
        auto* current_read = read_buffer_.load(std::memory_order_acquire);
        
        // 交换读写缓冲区
        write_buffer_.store(current_read, std::memory_order_release);
        read_buffer_.store(current_write, std::memory_order_release);
        
        // 清空新的写缓冲区
        current_read->clear();
        read_index_.store(0, std::memory_order_release);
        
        buffer_swapped_.store(true, std::memory_order_release);
    }
    
    // 消费者端：读取数据
    bool dequeue(T& item) {
        auto* current_read_buffer = read_buffer_.load(std::memory_order_acquire);
        size_t current_read_index = read_index_.load(std::memory_order_relaxed);
        
        if (current_read_index >= current_read_buffer->size()) {
            return false;  // 读缓冲区已空
        }
        
        item = std::move((*current_read_buffer)[current_read_index]);
        read_index_.store(current_read_index + 1, std::memory_order_release);
        return true;
    }
    
    // 检查是否有新数据可读
    bool has_data() const {
        auto* current_read_buffer = read_buffer_.load(std::memory_order_acquire);
        size_t current_read_index = read_index_.load(std::memory_order_acquire);
        return current_read_index < current_read_buffer->size();
    }
    
    // 检查写缓冲区是否已满
    bool write_buffer_full() const {
        auto* current_write_buffer = write_buffer_.load(std::memory_order_acquire);
        return current_write_buffer->size() >= max_size_;
    }
    
    // 获取写缓冲区当前大小
    size_t write_buffer_size() const {
        auto* current_write_buffer = write_buffer_.load(std::memory_order_acquire);
        return current_write_buffer->size();
    }
    
    // 获取读缓冲区剩余数据量
    size_t read_buffer_remaining() const {
        auto* current_read_buffer = read_buffer_.load(std::memory_order_acquire);
        size_t current_read_index = read_index_.load(std::memory_order_acquire);
        return current_read_buffer->size() - current_read_index;
    }
    
    // 获取容量
    size_t capacity() const {
        return max_size_;
    }
    
    // 检查缓冲区是否已切换
    bool buffer_was_swapped() {
        return buffer_swapped_.exchange(false, std::memory_order_acq_rel);
    }
}; 