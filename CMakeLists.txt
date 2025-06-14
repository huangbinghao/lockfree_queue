cmake_minimum_required(VERSION 3.10)
project(LockFreeQueue)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# 线程库
find_package(Threads REQUIRED)

# 包含头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 示例程序
add_executable(example example.cpp)
target_link_libraries(example Threads::Threads)

# 性能测试程序
add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark Threads::Threads)

# # 设置输出目录
# set_target_properties(example benchmark PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
# )

# # 安装规则
# install(TARGETS example benchmark
#     RUNTIME DESTINATION bin
# )

install(FILES 
    spsc_lockfree_queue.hpp
    locked_queue.hpp
    double_buffer_spsc.hpp
    DESTINATION include
) 