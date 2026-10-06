# 斗地主约64 MB CPU模型

目标机器：AMD Pro A10-8770（4个CPU核心）、3.8 GiB RAM、Ubuntu 20.04。由于OpenCL设备不可用，项目不再依赖GPU。

## 为什么不能把64 MB网络对每个动作都完整运行

64 MB FP32模型约有1600万参数。如果给64个候选动作分别运行一次，需要约10亿次乘加，A10-8770的延迟会很高。

当前改为共享结构：

```text
局面干线（每回合只算一次）
40 -> 512 -> 4096 -> 3072 -> 512

动作头（对Top-64分别计算）
[512维局面向量 + 40维动作向量] -> 256 -> 1
```

参数总量：

- 16,423,425个FP32参数
- 纯参数数据：65,693,700字节
- 约62.65 MiB（通常所说的约64 MB）

共享干线每回合只执行一次，随后用很小的动作头评价Top-64，在CPU上比“每个动作跑完整模型”高效得多。

## 推理流程

1. 规则AI流式枚举合法动作，候选最多4096个。
2. 规则评分预筛选Top-64。
3. 大型局面编码器只运行一次。
4. 64个小动作头由OpenMP分配给4个CPU核心。
5. 规则分和网络分混合选择最终动作。
6. 没有模型文件时不分配64 MB权重，也不执行神经网络计算。

## 编译

```bash
sudo apt install g++ cmake libomp-dev
cmake -S . -B build -DDDZ_OPENMP=ON -DDDZ_NATIVE=ON
cmake --build build -j4
OMP_NUM_THREADS=4 ./build/doudizhu
```

或者：

```bash
g++ -std=c++17 -O3 -march=native \
  -fopenmp -DDDZ_USE_OPENMP=1 \
  -Wall -Wextra -pedantic main.cpp -o doudizhu
OMP_NUM_THREADS=4 ./doudizhu
```

A10-8770只有4个通用CPU核心，建议固定：

```bash
export OMP_NUM_THREADS=4
export OMP_PROC_BIND=true
```

## 权重文件DDZMLP4

加载方法：

```bash
DDZ_MODEL=./doudizhu_64m.bin OMP_NUM_THREADS=4 ./doudizhu
```

文件为8字节头 `DDZMLP4\0`，之后依次保存little-endian float32：

1. `sw1`：512 × 40，`sb1`：512
2. `sw2`：4096 × 512，`sb2`：4096
3. `sw3`：3072 × 4096，`sb3`：3072
4. `sw4`：512 × 3072，`sb4`：512
5. `hw1`：256 × 552，`hb1`：256
6. `hw2`：256，`hb2`：1

旧版DDZMLP1～3权重不会被错误加载。

## 内存预算

- 权重：约62.65 MiB
- 最大输入特征批次：约18 KiB（Top-64 × 72 × 4）
- 中间激活：不到100 KiB
- 候选牌及游戏状态：通常数MiB以内
- 3.8 GiB内存足够运行

训练比推理更占内存：Adam需要参数、梯度和两组动量，单模型训练状态可能超过250 MB。仍能运行，但Replay Buffer应限制在20,000～50,000条，并避免同时启动过多自对弈进程。
