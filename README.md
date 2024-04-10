## 介绍

本项目是linux上基于c++23的轻量kv存储引擎

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
cd build/tinyKv
./tinyKv
```
