#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <iomanip>
#include <functional>
#include <cmath>
#include <cstring>
#include <algorithm>

#include "spsc_lockfree_queue.hpp"
#include "locked_queue.hpp"
#include "double_buffer_spsc.hpp"

// 测试配置
struct BenchmarkConfig {
    size_t num_operations = 1000000;  // 操作次数
    size_t queue_size = 1024;         // 队列大小
    size_t warmup_operations = 10000; // 预热操作次数
    int num_runs = 5;                 // 每个测试运行次数
};

// 性能统计结果
struct BenchmarkResult {
    std::string name;
    double avg_throughput_ops_per_sec;
    double avg_latency_ns;
    double min_latency_ns;
    double max_latency_ns;
    double p95_latency_ns;
    double p99_latency_ns;
    std::vector<double> latencies;
    
    void calculate_stats() {
        if (latencies.empty()) return;
        
        std::sort(latencies.begin(), latencies.end());
        min_latency_ns = latencies.front();
        max_latency_ns = latencies.back();
        
        size_t p95_idx = static_cast<size_t>(latencies.size() * 0.95);
        size_t p99_idx = static_cast<size_t>(latencies.size() * 0.99);
        
        p95_latency_ns = latencies[std::min(p95_idx, latencies.size() - 1)];
        p99_latency_ns = latencies[std::min(p99_idx, latencies.size() - 1)];
        
        double sum = 0.0;
        for (double latency : latencies) {
            sum += latency;
        }
        avg_latency_ns = sum / latencies.size();
    }
};

// 高精度计时器
class HighResTimer {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    
public:
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    double elapsed_ns() const {
        auto end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::nano>(end_time - start_time).count();
    }
    
    double elapsed_ms() const {
        return elapsed_ns() / 1000000.0;
    }
};

// 测试数据结构
struct TestData {
    uint64_t id;
    uint64_t timestamp;
    char padding[48];  // 填充到64字节，避免false sharing
    
    TestData() : id(0), timestamp(0) {
        memset(padding, 0, sizeof(padding));
    }
    
    TestData(uint64_t i, uint64_t ts) : id(i), timestamp(ts) {
        memset(padding, 0, sizeof(padding));
    }
};

// SPSC无锁队列测试
BenchmarkResult benchmark_spsc_lockfree(const BenchmarkConfig& config) {
    BenchmarkResult result;
    result.name = "SPSC Lock-Free Queue";
    
    std::vector<double> all_latencies;
    std::vector<double> throughputs;
    
    for (int run = 0; run < config.num_runs; ++run) {
        SPSCLockFreeQueue<TestData, 2048> queue;  // 必须是2的幂次
        std::atomic<bool> producer_done{false};
        std::atomic<size_t> items_consumed{0};
        
        std::vector<double> run_latencies;
        run_latencies.reserve(config.num_operations);
        
        HighResTimer total_timer;
        
        // 生产者线程
        std::thread producer([&]() {
            HighResTimer timer;
            
            // 预热
            for (size_t i = 0; i < config.warmup_operations; ++i) {
                TestData data(i, std::chrono::high_resolution_clock::now().time_since_epoch().count());
                while (!queue.enqueue(data)) {
                    std::this_thread::yield();
                }
            }
            
            // 实际测试
            total_timer.start();
            for (size_t i = 0; i < config.num_operations; ++i) {
                timer.start();
                TestData data(i, std::chrono::high_resolution_clock::now().time_since_epoch().count());
                
                while (!queue.enqueue(data)) {
                    std::this_thread::yield();
                }
                
                run_latencies.push_back(timer.elapsed_ns());
            }
            producer_done.store(true);
        });
        
        // 消费者线程
        std::thread consumer([&]() {
            TestData data;
            size_t consumed = 0;
            
            // 预热
            while (consumed < config.warmup_operations) {
                if (queue.dequeue(data)) {
                    consumed++;
                } else {
                    std::this_thread::yield();
                }
            }
            
            // 实际测试
            consumed = 0;
            while (!producer_done.load() || !queue.empty()) {
                if (queue.dequeue(data)) {
                    consumed++;
                }
            }
            items_consumed.store(consumed);
        });
        
        producer.join();
        consumer.join();
        
        double total_time_ms = total_timer.elapsed_ms();
        double throughput = (config.num_operations / total_time_ms) * 1000.0;
        throughputs.push_back(throughput);
        
        all_latencies.insert(all_latencies.end(), run_latencies.begin(), run_latencies.end());
    }
    
    // 计算平均吞吐量
    double sum_throughput = 0.0;
    for (double tp : throughputs) {
        sum_throughput += tp;
    }
    result.avg_throughput_ops_per_sec = sum_throughput / throughputs.size();
    
    result.latencies = std::move(all_latencies);
    result.calculate_stats();
    
    return result;
}

// 有锁队列测试
BenchmarkResult benchmark_locked_queue(const BenchmarkConfig& config) {
    BenchmarkResult result;
    result.name = "Locked Queue";
    
    std::vector<double> all_latencies;
    std::vector<double> throughputs;
    
    for (int run = 0; run < config.num_runs; ++run) {
        LockedQueue<TestData> queue(config.queue_size);
        std::atomic<bool> producer_done{false};
        std::atomic<size_t> items_consumed{0};
        
        std::vector<double> run_latencies;
        run_latencies.reserve(config.num_operations);
        
        HighResTimer total_timer;
        
        // 生产者线程
        std::thread producer([&]() {
            HighResTimer timer;
            
            // 预热
            for (size_t i = 0; i < config.warmup_operations; ++i) {
                TestData data(i, std::chrono::high_resolution_clock::now().time_since_epoch().count());
                while (!queue.enqueue(data)) {
                    std::this_thread::yield();
                }
            }
            
            // 实际测试
            total_timer.start();
            for (size_t i = 0; i < config.num_operations; ++i) {
                timer.start();
                TestData data(i, std::chrono::high_resolution_clock::now().time_since_epoch().count());
                
                while (!queue.enqueue(data)) {
                    std::this_thread::yield();
                }
                
                run_latencies.push_back(timer.elapsed_ns());
            }
            producer_done.store(true);
        });
        
        // 消费者线程
        std::thread consumer([&]() {
            TestData data;
            size_t consumed = 0;
            
            // 预热
            while (consumed < config.warmup_operations) {
                if (queue.dequeue(data)) {
                    consumed++;
                } else {
                    std::this_thread::yield();
                }
            }
            
            // 实际测试
            consumed = 0;
            while (!producer_done.load() || !queue.empty()) {
                if (queue.dequeue(data)) {
                    consumed++;
                } else {
                    std::this_thread::yield();
                }
            }
            items_consumed.store(consumed);
        });
        
        producer.join();
        consumer.join();
        
        double total_time_ms = total_timer.elapsed_ms();
        double throughput = (config.num_operations / total_time_ms) * 1000.0;
        throughputs.push_back(throughput);
        
        all_latencies.insert(all_latencies.end(), run_latencies.begin(), run_latencies.end());
    }
    
    // 计算平均吞吐量
    double sum_throughput = 0.0;
    for (double tp : throughputs) {
        sum_throughput += tp;
    }
    result.avg_throughput_ops_per_sec = sum_throughput / throughputs.size();
    
    result.latencies = std::move(all_latencies);
    result.calculate_stats();
    
    return result;
}

// 双缓冲SPSC测试
BenchmarkResult benchmark_double_buffer(const BenchmarkConfig& config) {
    BenchmarkResult result;
    result.name = "Double Buffer SPSC";
    
    std::vector<double> all_latencies;
    std::vector<double> throughputs;
    
    for (int run = 0; run < config.num_runs; ++run) {
        DoubleBufferSPSC<TestData> queue(config.queue_size);
        std::atomic<bool> producer_done{false};
        std::atomic<size_t> items_consumed{0};
        
        std::vector<double> run_latencies;
        run_latencies.reserve(config.num_operations);
        
        HighResTimer total_timer;
        
        // 生产者线程
        std::thread producer([&]() {
            HighResTimer timer;
            size_t batch_size = config.queue_size / 4;  // 批处理大小
            
            // 预热
            for (size_t i = 0; i < config.warmup_operations; ++i) {
                TestData data(i, std::chrono::high_resolution_clock::now().time_since_epoch().count());
                while (!queue.enqueue(data)) {
                    queue.swap_buffers();
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
                
                if (i % batch_size == 0) {
                    queue.swap_buffers();
                }
            }
            
            // 实际测试
            total_timer.start();
            for (size_t i = 0; i < config.num_operations; ++i) {
                timer.start();
                TestData data(i, std::chrono::high_resolution_clock::now().time_since_epoch().count());
                
                while (!queue.enqueue(data)) {
                    queue.swap_buffers();
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
                
                run_latencies.push_back(timer.elapsed_ns());
                
                // 定期切换缓冲区
                if (i % batch_size == 0) {
                    queue.swap_buffers();
                }
            }
            
            // 最后切换一次确保数据可被消费
            queue.swap_buffers();
            producer_done.store(true);
        });
        
        // 消费者线程
        std::thread consumer([&]() {
            TestData data;
            size_t consumed = 0;
            
            // 预热
            while (consumed < config.warmup_operations) {
                if (queue.dequeue(data)) {
                    consumed++;
                } else {
                    std::this_thread::yield();
                }
            }
            
            // 实际测试
            consumed = 0;
            while (!producer_done.load() || queue.has_data()) {
                if (queue.dequeue(data)) {
                    consumed++;
                } else {
                    std::this_thread::yield();
                }
            }
            items_consumed.store(consumed);
        });
        
        producer.join();
        consumer.join();
        
        double total_time_ms = total_timer.elapsed_ms();
        double throughput = (config.num_operations / total_time_ms) * 1000.0;
        throughputs.push_back(throughput);
        
        all_latencies.insert(all_latencies.end(), run_latencies.begin(), run_latencies.end());
    }
    
    // 计算平均吞吐量
    double sum_throughput = 0.0;
    for (double tp : throughputs) {
        sum_throughput += tp;
    }
    result.avg_throughput_ops_per_sec = sum_throughput / throughputs.size();
    
    result.latencies = std::move(all_latencies);
    result.calculate_stats();
    
    return result;
}

// 打印测试结果
void print_results(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "队列性能对比测试结果" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    
    // 表头
    std::cout << std::left;
    std::cout << std::setw(20) << "队列类型"
              << std::setw(15) << "吞吐量(ops/s)"
              << std::setw(12) << "平均延迟(ns)"
              << std::setw(12) << "最小延迟(ns)"
              << std::setw(12) << "最大延迟(ns)"
              << std::setw(12) << "P95延迟(ns)"
              << std::setw(12) << "P99延迟(ns)" << std::endl;
    
    std::cout << std::string(100, '-') << std::endl;
    
    // 数据行
    for (const auto& result : results) {
        std::cout << std::setw(20) << result.name
                  << std::setw(15) << std::fixed << std::setprecision(0) << result.avg_throughput_ops_per_sec
                  << std::setw(12) << std::fixed << std::setprecision(1) << result.avg_latency_ns
                  << std::setw(12) << std::fixed << std::setprecision(1) << result.min_latency_ns
                  << std::setw(12) << std::fixed << std::setprecision(1) << result.max_latency_ns
                  << std::setw(12) << std::fixed << std::setprecision(1) << result.p95_latency_ns
                  << std::setw(12) << std::fixed << std::setprecision(1) << result.p99_latency_ns << std::endl;
    }
    
    std::cout << std::string(100, '=') << std::endl;
    
    // 性能对比分析
    if (results.size() >= 2) {
        std::cout << "\n性能对比分析：" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        const auto& lockfree = results[0];  // SPSC Lock-Free
        const auto& locked = results[1];    // Locked Queue
        
        double throughput_improvement = ((lockfree.avg_throughput_ops_per_sec - locked.avg_throughput_ops_per_sec) 
                                       / locked.avg_throughput_ops_per_sec) * 100.0;
        
        double latency_improvement = ((locked.avg_latency_ns - lockfree.avg_latency_ns) 
                                    / locked.avg_latency_ns) * 100.0;
        
        std::cout << "无锁队列 vs 有锁队列：" << std::endl;
        std::cout << "  吞吐量提升: " << std::fixed << std::setprecision(1) << throughput_improvement << "%" << std::endl;
        std::cout << "  延迟降低: " << std::fixed << std::setprecision(1) << latency_improvement << "%" << std::endl;
        
        if (results.size() >= 3) {
            const auto& double_buffer = results[2];  // Double Buffer
            
            double db_throughput_vs_lockfree = ((double_buffer.avg_throughput_ops_per_sec - lockfree.avg_throughput_ops_per_sec) 
                                              / lockfree.avg_throughput_ops_per_sec) * 100.0;
            
            double db_latency_vs_lockfree = ((lockfree.avg_latency_ns - double_buffer.avg_latency_ns) 
                                           / lockfree.avg_latency_ns) * 100.0;
            
            std::cout << "\n双缓冲 vs 无锁队列：" << std::endl;
            std::cout << "  吞吐量差异: " << std::fixed << std::setprecision(1) << db_throughput_vs_lockfree << "%" << std::endl;
            std::cout << "  延迟差异: " << std::fixed << std::setprecision(1) << db_latency_vs_lockfree << "%" << std::endl;
        }
    }
}

int main() {
    std::cout << "SPSC队列性能对比测试" << std::endl;
    std::cout << "正在运行性能测试，请稍等..." << std::endl;
    
    BenchmarkConfig config;
    config.num_operations = 1000000;
    config.queue_size = 1024;
    config.warmup_operations = 10000;
    config.num_runs = 3;
    
    std::cout << "\n测试配置：" << std::endl;
    std::cout << "  操作次数: " << config.num_operations << std::endl;
    std::cout << "  队列大小: " << config.queue_size << std::endl;
    std::cout << "  预热操作: " << config.warmup_operations << std::endl;
    std::cout << "  运行次数: " << config.num_runs << std::endl;
    
    std::vector<BenchmarkResult> results;
    
    std::cout << "\n正在测试 SPSC Lock-Free Queue..." << std::endl;
    results.push_back(benchmark_spsc_lockfree(config));
    
    std::cout << "正在测试 Locked Queue..." << std::endl;
    results.push_back(benchmark_locked_queue(config));
    
    std::cout << "正在测试 Double Buffer SPSC..." << std::endl;
    results.push_back(benchmark_double_buffer(config));
    
    print_results(results);
    
    return 0;
} 