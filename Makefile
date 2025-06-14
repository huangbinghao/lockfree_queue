# Makefile for SPSC Lock-Free Queue Project

# 编译器设置
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -march=native -pthread
DEBUGFLAGS = -g -O0 -DDEBUG
INCLUDES = -I.

# 目标文件
TARGETS = example benchmark
BINDIR = bin

# 默认目标
all: $(BINDIR) $(TARGETS)

# 创建bin目录
$(BINDIR):
	mkdir -p $(BINDIR)

# 编译示例程序
example: example.cpp spsc_lockfree_queue.hpp locked_queue.hpp double_buffer_spsc.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(BINDIR)/$@ $<

# 编译性能测试程序
benchmark: benchmark.cpp spsc_lockfree_queue.hpp locked_queue.hpp double_buffer_spsc.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(BINDIR)/$@ $<

# Debug版本
debug: CXXFLAGS += $(DEBUGFLAGS)
debug: $(BINDIR) $(TARGETS)

# 运行示例
run-example: example
	./$(BINDIR)/example

# 运行性能测试
run-benchmark: benchmark
	./$(BINDIR)/benchmark

# 清理
clean:
	rm -rf $(BINDIR)

# 安装（需要sudo权限）
install: all
	install -d /usr/local/bin
	install -m 755 $(BINDIR)/example /usr/local/bin/
	install -m 755 $(BINDIR)/benchmark /usr/local/bin/
	install -d /usr/local/include/lockfree_queue
	install -m 644 *.hpp /usr/local/include/lockfree_queue/

# 卸载
uninstall:
	rm -f /usr/local/bin/example
	rm -f /usr/local/bin/benchmark
	rm -rf /usr/local/include/lockfree_queue

# 代码格式化（需要clang-format）
format:
	clang-format -i *.cpp *.hpp

# 静态分析（需要cppcheck）
analyze:
	cppcheck --enable=all --std=c++17 *.cpp *.hpp

# 帮助信息
help:
	@echo "可用的Make目标："
	@echo "  all          - 编译所有程序（默认）"
	@echo "  debug        - 编译Debug版本"
	@echo "  example      - 编译示例程序"
	@echo "  benchmark    - 编译性能测试程序"
	@echo "  run-example  - 运行示例程序"
	@echo "  run-benchmark- 运行性能测试"
	@echo "  clean        - 清理编译文件"
	@echo "  install      - 安装到系统（需要sudo）"
	@echo "  uninstall    - 从系统卸载"
	@echo "  format       - 代码格式化（需要clang-format）"
	@echo "  analyze      - 静态分析（需要cppcheck）"
	@echo "  help         - 显示此帮助信息"

.PHONY: all debug clean install uninstall run-example run-benchmark format analyze help 