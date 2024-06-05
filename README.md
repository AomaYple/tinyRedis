## 介绍

本项目是linux上基于c++和io_uring的轻量级Redis

## 数据结构

使用跳表作为核心数据结构，支持redis的五种数据类型：字符串，哈希，列表，集合，有序集合

## 命令

支持redis的五种数据类型的基本操作命令，基于读写锁保证命令的原子性，支持事务

## 数据持久化

服务端关闭时，将数据持久化到磁盘，服务端启动时，从磁盘加载数据

## 日志

利用io_uring的异步io和linux O_APPEND特性实现了异步且线程安全的高性能日志系统，支持多种日志级别和提供详细的日志信息

## 协程

包装c++20协程的coroutine，实现了Awaiter和Task，简化异步编程

## io_uring

利用io_uring实现了高性能的异步io，支持多个io操作的批量提交，减少系统调用次数，提高性能

## 调度器

基于协程实现了一个简单的调度器，支持协程的创建、销毁、挂起和唤醒，程序会根据cpu核心数创建相应数量的调度器，每个调度器互相独立，互不干扰

## 信号处理

服务器会处理SIGTERM和SIGINT信号，优雅地关闭服务器

## 环境

gcc14以上，cmake，ninja，liburing2.4以上

## 编译

```shell 
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cd build
ninja
```

## 运行

服务端

```shell
cd build/tinyRedis/tinyRedisServer
./tinyRedisServer
```

客户端

```shell
cd build/tinyRedis/tinyRedisClient
./tinyRedisClient
```
