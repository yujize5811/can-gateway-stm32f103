# can_gw —— CAN-GW 应用层协议模块

纯 C99 实现，**不依赖 HAL / 芯片**，移植时只实现两个钩子函数。

## 1. 文件结构

```
can_gw/
├── include/
│   ├── can_gw.h            公共 API 与参数
│   ├── can_gw_internal.h   模块内部声明
│   ├── proto_rx.h          接收重组
│   └── crc.h               CRC8 / CRC32
└── src/
    ├── can_gw.c            初始化、发送状态机、收帧分发、统计
    ├── proto_rx.c          FF/CF 重组状态机
    └── crc.c               CRC 实现
```

## 2. 你只需要实现两个函数

```c
/* 把一帧交给 CAN 控制器，返回 true 表示已塞进邮箱 */
bool gwg_hw_send(uint32_t id, const uint8_t *data, uint8_t dlc);

/* 收到一包完整、校验过的用户数据 */
void gwg_on_payload(const uint8_t *data, uint16_t len);
```

参考实现见 `examples/stm32f1_port_example.c`。

## 3. 接入步骤

1. 把整个 `can_gw/` 目录加到 Keil 工程的组里，`include/` 加进 Include Paths；
2. 实现上面两个钩子（示例文件直接抄）；
3. `main()` 里：

```c
gwg_init(1);                       /* 本节点号：1 或 2 */

while (1) {
    gwg_poll(HAL_GetTick());       /* 建议 1~10 ms 调一次 */
}
```

4. CAN 接收中断里：

```c
gwg_rx_frame(hdr.StdId, rx_data, hdr.DLC, HAL_GetTick());
```

5. 发送：

```c
uint8_t buf[16] = "hello";
gwg_send(buf, 5, HAL_GetTick());   /* 失败返回 false（上一次还没发完） */
```

## 4. API 一览

| 函数 | 说明 |
|---|---|
| `gwg_init(node)` | 初始化，node 为 1 或 2 |
| `gwg_poll(now_ms)` | 状态机驱动，主循环周期调用 |
| `gwg_rx_frame(id, d, dlc, now)` | 中断入口，只做搬运和状态推进 |
| `gwg_send(data, len, now)` | 发送 ≤128 字节数据，忙时返回 false |
| `gwg_busy()` | 是否正在传输 |
| `gwg_stats()` | 取统计结构体指针 |
| `gwg_stats_csv(buf, len, now)` | 输出一行 CSV |

## 5. 协议要点

- 单帧（≤6 字节）走 `0x200`，不做 ACK；
- 大于 6 字节走 `0x400` 分片，**停等协议**：发一帧 → 等 ACK → 下一帧；
- 每帧带 CRC8，整包带 CRC32（追加在负载末尾 4 字节）；
- 超时 50 ms，最多重传 3 次，超限记错误并释放；
- 接收端逐帧回 ACK/NACK，序号错回 `GWG_ACK_SEQERR` 触发重发。

## 6. 状态机

```
发送：IDLE --gwg_send--> 发 FF --ACK--> 发 CF --ACK--> ... --> 收完 --> IDLE
               ↑______________超时/重传______________|
接收：IDLE --FF--> ASSEMBLING --CF(seq+)--> ... --> CRC32 --> 回调
```

## 7. 已验证

用 Arm Compiler 6.24 编译通过（`-std=c99 -Wall -Wextra`，无警告）：

```
OK  can_gw.c
OK  crc.c
OK  proto_rx.c
```

## 8. 待你补充的部分

- [ ] `gwg_hw_send` 的 HAL 实现（含邮箱满的处理）
- [ ] CAN 过滤器配置：只放行 `0x100~0x5FF`
- [ ] 心跳帧的周期发送（1 Hz）
- [ ] Bus-Off 检测与恢复（读 `CAN_ESR`）
- [ ] 滑动窗口 v2（把停等换成窗口 4）
