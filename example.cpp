#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

#include "spsc_lockfree_queue.hpp"
#include "locked_queue.hpp"
#include "double_buffer_spsc.hpp"

// 简单的演示数据
struct Message {
    int id;
    std::string content;
    
    Message() : id(0) {}
    Message(int i, const std::string& c) : id(i), content(c) {}
};

// SPSC无锁队列演示
void demo_spsc_lockfree() {
    std::cout << "\n=== SPSC无锁队列演示 ===" << std::endl;
    
    SPSCLockFreeQueue<Message, 16> queue;
    std::atomic<bool> done{false};
    
    // 生产者线程
    std::thread producer([&]() {
        for (int i = 0; i < 10; ++i) {
            Message msg(i, "Hello from producer " + std::to_string(i));
            
            while (!queue.enqueue(msg)) {
                std::this_thread::yield();  // 队列满时等待
            }
            
            std::cout << "生产者: 发送消息 " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        done.store(true);
    });
    
    // 消费者线程
    std::thread consumer([&]() {
        Message msg;
        int received = 0;
        
        while (!done.load() || !queue.empty()) {
            if (queue.dequeue(msg)) {
                std::cout << "消费者: 接收消息 " << msg.id << " - " << msg.content << std::endl;
                received++;
            } else {
                std::this_thread::yield();
            }
        }
        
        std::cout << "消费者总共接收了 " << received << " 条消息" << std::endl;
    });
    
    producer.join();
    consumer.join();
}

// 有锁队列演示
void demo_locked_queue() {
    std::cout << "\n=== 有锁队列演示 ===" << std::endl;
    
    LockedQueue<Message> queue(16);
    std::atomic<bool> done{false};
    
    // 生产者线程
    std::thread producer([&]() {
        for (int i = 0; i < 10; ++i) {
            Message msg(i, "Hello from locked producer " + std::to_string(i));
            
            while (!queue.enqueue(msg)) {
                std::this_thread::yield();  // 队列满时等待
            }
            
            std::cout << "生产者: 发送消息 " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        done.store(true);
    });
    
    // 消费者线程
    std::thread consumer([&]() {
        Message msg;
        int received = 0;
        
        while (!done.load() || !queue.empty()) {
            if (queue.dequeue(msg)) {
                std::cout << "消费者: 接收消息 " << msg.id << " - " << msg.content << std::endl;
                received++;
            } else {
                std::this_thread::yield();
            }
        }
        
        std::cout << "消费者总共接收了 " << received << " 条消息" << std::endl;
    });
    
    producer.join();
    consumer.join();
}

// 双缓冲SPSC演示
void demo_double_buffer() {
    std::cout << "\n=== 双缓冲SPSC演示 ===" << std::endl;
    
    DoubleBufferSPSC<Message> queue(16);
    std::atomic<bool> done{false};
    
    // 生产者线程
    std::thread producer([&]() {
        for (int i = 0; i < 10; ++i) {
            Message msg(i, "Hello from double buffer producer " + std::to_string(i));
            
            while (!queue.enqueue(msg)) {
                queue.swap_buffers();  // 缓冲区满时切换
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            std::cout << "生产者: 发送消息 " << i << std::endl;
            
            // 每3个消息切换一次缓冲区
            if (i % 3 == 2) {
                queue.swap_buffers();
                std::cout << "生产者: 切换缓冲区" << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // 最后切换一次确保所有数据可被消费
        queue.swap_buffers();
        done.store(true);
    });
    
    // 消费者线程
    std::thread consumer([&]() {
        Message msg;
        int received = 0;
        
        while (!done.load() || queue.has_data()) {
            if (queue.dequeue(msg)) {
                std::cout << "消费者: 接收消息 " << msg.id << " - " << msg.content << std::endl;
                received++;
            } else {
                std::this_thread::yield();
            }
        }
        
        std::cout << "消费者总共接收了 " << received << " 条消息" << std::endl;
    });
    
    producer.join();
    consumer.join();
}

int main() {
    std::cout << "SPSC队列实现演示" << std::endl;
    std::cout << "==================" << std::endl;
    
    // 依次演示三种队列实现
    demo_spsc_lockfree();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    demo_locked_queue();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    demo_double_buffer();
    
    std::cout << "\n演示完成！" << std::endl;
    return 0;
} 