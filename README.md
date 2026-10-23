# CAN 总线双节点通信网关（STM32F103）

> 在 500 kbps CAN 总线上实现带 ACK 重传的应用层协议，支持多帧传输与 Bus-Off 自动恢复。

![status](https://img.shields.io/badge/status-WIP-orange)
![mcu](https://img.shields.io/badge/MCU-STM32F103C8T6-blue)
![can](https://img.shields.io/badge/CAN-500kbps-green)

> 当前状态：**仓库骨架已建，代码开发中**。
> 进度见下方 Roadmap。

## 1. 它解决什么问题

两块 F103 之间需要可靠传输超过 8 字节的业务数据。标准 CAN 只提供单帧收发，
本项目在其上设计 **CAN-GW 应用层协议**，解决分片、丢帧、重复、乱序四个问题，
并保证节点在总线故障后能自动恢复。

## 2. 系统结构

```
 ┌──────────┐    CAN 500kbps    ┌──────────┐    UART     ┌────────┐
 │ 节点 A   │◄─────────────────►│ 网关 B   │◄───────────►│ 上位机 │
 │ 20ms采样 │   120Ω     120Ω   │ 转发/命令│             │ Python │
 └──────────┘                   └──────────┘             └────────┘
```

## 3. Roadmap

- [ ] W1 单板回环自测（Loopback）
- [ ] W1 双节点 Normal 模式互发 + 过滤器
- [ ] W2 协议 v1：单帧 + CRC8
- [ ] W2 多帧分片 + 整包 CRC32
- [ ] W2 停等重传（ACK/超时/NACK）
- [ ] W2 统计模块（CSV 输出）
- [ ] W3 Bus-Off 检测与自动恢复
- [ ] W3 心跳与离线检测
- [ ] W3 故障注入测试（6 类）
- [ ] W4 协议 v2：滑动窗口（吞吐 20 → 100 帧/s）
- [ ] W4 72 h 长跑测试

## 4. 硬件

| 物料 | 数量 | 说明 |
|---|---|---|
| STM32F103C8T6 最小系统板 | 2 | 节点 A / 网关 B |
| TJA1050 CAN 收发模块 | 2 | STB 引脚必须拉低，否则不收发 |
| 120Ω 电阻 | 2 | 总线两端各一个 |
| USB-TTL | 1 | 串口调试 / 统计输出 |

接线见 [hardware/wiring.md](hardware/wiring.md)。

## 5. 目录结构

```
docs/        协议文档、测试报告、图片
firmware/    Keil 工程（CubeMX 生成）
hardware/    接线与硬件说明
tools/       上位机脚本
```

## 6. 快速开始

1. 按 [firmware/README.md](firmware/README.md) 用 CubeMX 生成工程
2. Keil 打开 `firmware/MDK-ARM/*.uvprojx`，F7 编译，F8 烧录
3. 串口 115200 查看日志

## 7. 测试数据

（开发中，完成后填入）

| 指标 | 目标 | 实测 |
|---|---|---|
| 连续运行 | 72 h | — |
| 累计帧数 | ≥ 50 万 | — |
| 丢帧 | 0 | — |
| 重传率 | < 0.1% | — |
| Bus-Off 恢复 | < 200 ms | — |
| 中断→任务延迟 P99 | < 500 µs | — |

## 8. 协议

详见 [docs/protocol.md](docs/protocol.md)。

## 9. 踩坑记录

（开发过程中持续补充，这是仓库最有价值的部分之一）

- 待补充

## 10. License

MIT
