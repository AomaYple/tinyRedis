## 介绍

本项目是linux上基于c++的轻量级Redis

## 环境

gcc，cmake，ninja，liburing

## 编译

```shell 
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cd build
ninja
```

## 运行

服务端

```shell
cd build/tinyRedisServer
./tinyRedisServer
```
