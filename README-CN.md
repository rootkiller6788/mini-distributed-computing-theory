# Mini Distributed Computing Theory（迷你分布式计算理论）

**从零开始、零依赖的 C 语言实现**，涵盖分布式计算理论中的经典结论与算法。每个模块将教科书中的定理——从 FLP 不可能性、拜占庭协议下界，到 LOCAL 模型轮复杂度、自稳定理论——转化为可独立运行、自包含的 C 代码。

## 子模块

| 子模块 | 主题 | 参考课程 |
|-----------|--------|-------------|
| [mini-byzantine-agreement-lower-bound](mini-byzantine-agreement-lower-bound/) | 拜占庭协议、f < N/3 下界、EIG 算法、Phase King、Ben-Or 随机化共识、Dolev-Strong 签名协议 | MIT 6.852, ETH 263-4650, Princeton COS 551 |
| [mini-coloring-mis-lower-bounds](mini-coloring-mis-lower-bounds/) | 分布式 (Δ+1)-染色、MIS（Luby 算法）、对称破缺、Cole-Vishkin、Linial log* n 下界 | MIT 6.852, CMU 15-855, ETH 263-4650 |
| [mini-consensus-fault-tolerance](mini-consensus-fault-tolerance/) | Paxos、Raft、PBFT、中本聪共识、Quorum 系统、崩溃/拜占庭故障模型 | MIT 6.852, Stanford CS254, CMU 15-712 |
| [mini-flp-impossibility-proof](mini-flp-impossibility-proof/) | FLP 不可能性定理、双价性证明、异步共识、配置图探索 | MIT 6.852, Stanford CS244E, CMU 15-712 |
| [mini-local-model-port-numbering](mini-local-model-port-numbering/) | LOCAL/CONGEST 模型、端口编号、Linial 下界、3-染色环、无汇定向 | MIT 6.852, ETH 263-4650, Aalto CS |
| [mini-paxos-raft-formal](mini-paxos-raft-formal/) | Paxos 与 Raft 的形式化验证、类 TLA+ 模型检测、归纳不变量、安全性证明 | MIT 6.852, Stanford CS254, Cambridge Part III |
| [mini-self-stabilization-dijkstra](mini-self-stabilization-dijkstra/) | Dijkstra 令牌环、守护进程/调度器模型、收敛分析、闭包证明 | MIT 6.841, Stanford CS254, ETH 263-4650 |
| [mini-synchronizer-alpha-beta-gamma](mini-synchronizer-alpha-beta-gamma/) | α/β/γ 同步器（Awerbuch 1985）、同步模拟、BFS、领导者选举、异步 MST | MIT 6.852, ETH 263-4650, Princeton COS 551 |

## 设计理念

- **零外部依赖** — 纯 C（C99/C11），仅使用 `libc`（必要时使用 `libm`）
- **模块自包含** — 每个目录自带 `Makefile`、`include/`、`src/`、`examples/`、`demos/`、`tests/`、`docs/`
- **理论到代码的映射** — 每个 `.h` 文件内嵌 L1–L8 知识层级和原始教材的课程对齐信息
- **可运行证明** — 不可能性证明（FLP、拜占庭协议下界）不止是文字描述，同时附带可执行的状态空间探索器，展示双价性构造过程

## 构建方式

每个模块相互独立。进入模块目录后运行：

```bash
cd mini-byzantine-agreement-lower-bound
make all    # 构建全部
make test   # 运行测试
```

需要 **GCC** 和 **GNU Make**。

## 项目结构

```
mini-distributed-computing-theory/
├── mini-byzantine-agreement-lower-bound/   # 拜占庭协议、f < N/3 下界、EIG、Phase King
├── mini-coloring-mis-lower-bounds/         # 分布式染色、MIS、对称破缺、Linial 下界
├── mini-consensus-fault-tolerance/         # Paxos、Raft、PBFT、Quorum 系统、故障模型
├── mini-flp-impossibility-proof/           # FLP 定理、双价性证明、异步不可能性
├── mini-local-model-port-numbering/        # LOCAL/CONGEST 模型、端口编号、轮复杂度
├── mini-paxos-raft-formal/                 # 形式化验证、类 TLA+ 模型检测、归纳不变量
├── mini-self-stabilization-dijkstra/       # Dijkstra 令牌环、守护进程模型、收敛分析
└── mini-synchronizer-alpha-beta-gamma/     # α/β/γ 同步器、同步模拟
```

## 许可证

MIT
