# AC5 → AC6 迁移排查清单

> 记录一次真实迁移：把一批用 ARM Compiler 5（AC5 / armcc）建的 Keil 工程迁到
> ARM Compiler 6（AC6 / armclang），共 35 个工程，最终全部 0 Error。
>
> 适用场景：Keil MDK 5.37 以后版本（不再自带 AC5）、老工程直接打开报一堆错、
> 或者把别人用旧版 Keil 建的工程拿到自己机器上编译。

---

## 0. 先确认根因：到底是"缺编译器"还是"语法不兼容"

打开 `.uvprojx`（XML，记事本可看），找这几行：

```xml
<pCCUsed>5060750::V5.06 update 6 (build 750)::ARMCC</pCCUsed>
<uAC6>0</uAC6>
```

| 现象 | 含义 |
|---|---|
| `pCCUsed` 是 `V5.06 ... ARMCC` | 工程指定用 AC5 编译 |
| `uAC6` 为 `0` | 工程未启用 AC6 |
| 本机 `Keil_v5\ARM\ARMCC` 目录不存在 | 本机根本没装 AC5 |

**这三条同时成立 = 工程会被 AC6 接管，然后按 AC5 的语法去编译 → 大面积报错。**

快速验证本机有没有 AC5：

```powershell
Test-Path "D:\Users\...\Keil_v5\ARM\ARMCC\bin\armcc.exe"   # False 表示没有 AC5
& "...\ARM\ARMCLANG\bin\armclang.exe" --version            # 有输出表示 AC6 可用
```

---

## 1. 症状速查表

按"报错原文 → 根因 → 修法"对照，**先解决第一个错误**，后面大量报错往往是它的连锁反应。

| 报错片段 | 根因 | 修法 |
|---|---|---|
| `expected 'volatile', 'inline', 'goto', or '('` + `unknown type name '__forceinline'` | 用了 AC5 专用移植层（`portable/RVDS/ARM_CM3/portmacro.h`），`__forceinline` 是 armcc 关键字 | 换成 GCC 移植层（见 §2） |
| `A3900U: Unrecognized option '-Wno-...'`（在 `.s` 文件上） | 编译开关被加到了**汇编器**（`<Aads>`），assembly 不认 C 的开关 | 开关只加到 `<Cads>`（见 §5） |
| `incompatible function pointer types passing 'void (void)' to parameter of type 'TaskFunction_t'` | 老代码把任务函数写成 `void task(void)`，AC6 要求 `void task(void *)` | 改函数签名，或加 `-Wno-incompatible-function-pointer-types` |
| `call to undeclared function 'vTaskDelay' / 'memset' / 'printf'` | `ISO C99 and later do not support implicit function declarations`，代码缺 `#include` | 补头文件，或加 `-Wno-implicit-function-declaration` |
| `expected identifier or '('` 指向一行注释 | 源码被改坏（注释结束符重复） | 手工修注释 |
| `L6050U: The code size of this image (N bytes) exceeds the maximum allowed for this version of the linker` | 没有授权，跑在 MDK-Lite 模式（32 KB 上限） | 激活授权（见 §7） |
| `L6406E / L6407E: No space in execution regions with .ANY selector` | 真的超出 Flash / RAM，或 Lite 的 32 KB 上限 | 看 §6 判断是哪一种 |
| `__asm void foo(void) { ... }` 编译不过 | AC5 的内嵌汇编语法，armclang 不支持 | 改用 GCC 内联汇编或官方 GCC port |
| `__irq`、`__swi`、`__align`、`__packed` 报错 | 部分 AC5 关键字在 AC6 不可用 | CMSIS 已提供宏（`__ALIGNED`、`__PACKED` 等），改用宏 |

---

## 2. 核心修复：FreeRTOS 移植层 RVDS → GCC

这是最典型、也是报错最多的一个。

**原因**：`portable/RVDS/ARM_CM3/` 是给 armcc 写的——`portmacro.h` 里用 `__forceinline`，
`port.c` 里用 `__asm { ... }` 嵌入式汇编，armclang 两者都不认。

**做法**（`port.c` 和 `portmacro.h` 两个文件都要换）：

1. 新建目录
   ```
   Middlewares\Third_Party\FreeRTOS\Source\portable\GCC\ARM_CM3\
   ```
2. 把 FreeRTOS 官方的 `GCC/ARM_CM3/port.c`、`portmacro.h` 拷进去
   （同版本内核的 GCC port，可以从 STM32Cube 的其它系列固件包里拷）
3. 改 `.uvprojx` 里两处路径：

   ```xml
   <!-- 文件引用 -->
   <FilePath>../Middlewares/Third_Party/FreeRTOS/Source/portable/RVDS/ARM_CM3/port.c</FilePath>
   <!-- 改成 -->
   <FilePath>../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3/port.c</FilePath>
   ```

   ```xml
   <!-- Include Path -->
   .../portable/RVDS/ARM_CM3;...   →   .../portable/GCC/ARM_CM3;...
   ```

> 注意 CM3 / CM4F / CM7 要和芯片内核对应；F103 是 Cortex-M3。

---

## 3. 任务函数签名

AC5 对这些很宽松，AC6 严格。两种处理：

**方式 A（推荐，改代码）**

```c
/* 旧 */
void my_task(void) { ... }
xTaskCreate(my_task, "t", ...);

/* 新 */
void my_task(void *arg) { (void)arg; ... }
```

**方式 B（不改代码，加开关）**

```c
-Wno-incompatible-function-pointer-types
```

> 本项目采用了方式 B，因为课程源码量太大；实际产品代码建议用方式 A。

---

## 4. 隐式函数声明

AC6 默认 `-std=c99` 及以上，**调用了没声明过的函数 = 编译错误**（AC5 只是 warning）。

```
error: call to undeclared function 'vTaskDelay'
```

两种处理：

- 补上正确的头文件（`#include "FreeRTOS.h"` / `#include "task.h"` / `<string.h>` / `<stdio.h>`）
- 或者加 `-Wno-implicit-function-declaration`（老代码无奈之举，返回值非 int 时有风险）

---

## 5. 编译开关要加对地方（Cads vs Aads）

`.uvprojx` 里每个 target 有三套工具选项：

```xml
<Cads>  <!-- C 编译器：-Wall、-O2、-DXXX 都加这里 -->
  <VariousControls>
    <MiscControls></MiscControls>
```

```xml
<Aads>  <!-- 汇编器：只能加汇编相关参数 -->
  <VariousControls>
    <MiscControls></MiscControls>
```

**踩坑记录**：一次批量替换把 C 开关写进了 `<Aads>`，结果每个工程都报

```
startup_stm32f103xb.s: error: A3900U: Unrecognized option '-no-implicit-function-declaration'.
```

正确做法是用脚本**只替换第一个 `<MiscControls>`**（对应 Cads），或者用正则把
`<Aads>...</Aads>` 块里的内容剔除。

---

## 6. 链接报错：是真超了，还是授权限制？

两个报错长得很像，含义完全不同：

| 报错 | 含义 |
|---|---|
| `L6050U: code size ... exceeds the maximum allowed for this version of the linker` | **一定是授权问题**（Lite 版 32 KB 上限），跟代码无关 |
| `L6406E / L6407E: No space in execution regions` | 可能是真超容量，也可能是 Lite 上限 |

**判断方法**：把分散加载区域临时放大，看能不能链接成功、真实大小是多少。

```
/* 临时改成 1 MB 的 scatter，只为量尺寸，不是最终配置 */
LR_IROM1 0x08000000 0x00100000 {
  ER_IROM1 0x08000000 0x00100000 { *.o (RESET, +First) .ANY (+RO) .ANY (+XO) }
  RW_IRAM1 0x20000000 0x00005000 { .ANY (+RW +ZI) }
}
```

用 `armlink --via 工程生成的 .lnp` 手动链接，读 `Program Size`：

```
Program Size: Code=42612 RO-data=11712 RW-data=4384 ZI-data=6792
              ROM 需求 ≈ Code + RO + RW = 58708 字节
```

再和芯片实际 Flash 比（F103C8T6 = 64 KB）。

> 血泪教训：链接器在放不下时报告的是"剩余没放下的部分"，**不是总溢出量**。
> 曾经看到 `0x60 bytes could not fit` 就以为只差 96 字节，实际用 1 MB 区域一量，
> 真实超出近 10 KB。**要结论就用量尺寸的办法，不要看那条报错。**

---

## 7. 授权：Lite 32 KB 上限

Keil 未授权时自动进入 **MDK-Lite**，链接上限 32 KB。

免费方案：**Keil MDK Community Edition**（Arm 官方，非商业用途免费，无代码大小限制）：

1. 打开 Keil → **File → License Management**
2. 切到 **User-Based License** 标签
3. **Activate / Deactivate...** → 打开 Arm License Management Utility
4. **License Server...** 填 `https://mdk-preview.keil.arm.com` → **Query**
5. 选 **Keil MDK Community** → **Activate**

也可以用命令行验证激活状态：

```powershell
& "…\Keil_v5\UV4\armlm\armlm.exe" inspect
```

---

## 8. 批量修复脚本思路

工程多的时候手工点不现实，`.uvprojx` 就是 XML，可以直接改：

| 字段 | 改成 |
|---|---|
| `<uAC6>` | `1` |
| `<pCCUsed>` | `6240000::V6.24::ARMCLANG`（版本号对应你装的 AC6） |
| `<IncludePath>` 里的 port 目录 | `RVDS` → `GCC` |
| `port.c` 的 `<FilePath>` | 同上 |
| `<Cads><VariousControls><MiscControls>` | 追加兼容开关 |

**注意事项**：

1. 改之前 `Copy-Item xxx.uvprojx xxx.uvprojx.bak`；
2. 只替换 `<Cads>` 下的 MiscControls（别碰 `<Aads>`）；
3. 改完用命令行批量编译验证，而不是一个个点 GUI：

   ```powershell
   & UV4.exe -r "path\to\project.uvprojx" -j0 -o "build.log"
   # 退出码 0/1 = 无错误（1 表示有 warning）
   ```

---

## 9. 迁移后自检清单

- [ ] 编译 0 Error（warning 可以接受，但要逐条看有没有真问题）
- [ ] `Program Size` 的 ROM/RAM 都在芯片规格内
- [ ] 烧录后能正常启动（不 HardFault）
- [ ] **中断还能进**（向量表位置正确，尤其是 Bootloader 场景要改 `SCB->VTOR`）
- [ ] 浮点/打印输出正常（MicroLIB 与标准库的行为差异）
- [ ] 把临时调试开关清掉（`-Wno-*` 只用于过渡，最终代码应修根因）

---

## 10. 一句话总结

> AC5 到 AC6 不是"换个编译器"这么简单：**移植层要换、语法要改、报错要认**。
> 排查顺序永远是：先看第一个错误 → 判断是语法、移植层还是授权 → 修完立刻编译验证。

---

*文档来源：2026-10 一个 STM32F103 FreeRTOS 课程工程的批量迁移实践（35 个工程）。*
