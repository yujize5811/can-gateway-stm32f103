# 固件工程（Keil MDK + CubeMX）

本目录存放 CubeMX 生成的 Keil 工程。**只提交源码与 `.uvprojx`，不要提交 `Objects/`、`.axf`、`.hex`**（`.gitignore` 已处理）。

## 1. CubeMX 配置要点

新建工程时按下面配置，避免后面返工：

| 项目 | 配置 | 原因 |
|---|---|---|
| 芯片 | STM32F103C8T6 | 64 KB Flash / 20 KB RAM |
| 时钟 | HSE 8 MHz → PLL ×9 → 72 MHz | 标准配置 |
| APB1 | 36 MHz | CAN 时钟源，位定时按此计算 |
| CAN1 | 500 kbps，BS1=6 / BS2=2 / BRP=8 | 36MHz/(8×9)=500kHz，采样点 77.8% |
| CAN 模式 | 先 **Loopback** 自测，通了再改 Normal | 单板也能验证收发链路 |
| CAN NVIC | 使能 RX0 中断，抢占优先级建议 2 | 避免被其他中断延迟 |
| USART1 | 115200 8N1 | 调试与统计输出 |
| Toolchain | MDK-ARM V5 | 与 Keil 匹配 |

生成的工程放在 `firmware/MDK-ARM/` 下即可。

## 2. 代码组织建议

```
Core/Src/          业务与 HAL 初始化（CubeMX 生成）
Middleware/can_gw/
  ├─ bsp_can.c     CAN 初始化、过滤器、发送邮箱
  ├─ can_irq.c     中断回调：收帧入环形缓冲
  ├─ ringbuf.c     单生产者单消费者环形缓冲
  ├─ proto_rx.c    拆帧、CRC、组包状态机
  ├─ proto_tx.c    分片、重传定时器
  └─ monitor.c     统计与 CSV 输出
```

## 3. 编译与烧录

1. Keil 打开 `MDK-ARM/*.uvprojx`
2. F7 编译，应 0 Error
3. F8 烧录（ST-Link）
4. 串口助手 115200 查看输出

## 4. 统计输出格式

每行 CSV，方便上位机解析：

```
t,tx,rx,retrans,err,tec,rec
1523,2048,2048,3,0,0,0
```

`tools/monitor.py` 会读取这个格式并统计。
