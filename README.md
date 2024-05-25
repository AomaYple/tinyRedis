## 介绍

本项目是linux上基于c++的轻量级Redis

## 环境

gcc，cmake，ninja

## 编译

```shell 
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cd build
ninja
```

## 运行

```shell
cd build/tinyKV
./tinyKV
```
