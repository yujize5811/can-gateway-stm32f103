#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CAN 网关上位机统计工具。

用法：
    python monitor.py stats test.csv        # 统计已有 CSV
    python monitor.py live COM3             # 实时读串口（需要 pyserial）

CSV 格式（固件每秒输出一行）：
    t,tx,rx,retrans,err,tec,rec
"""
import sys
import csv
import argparse


def read_rows(stream):
    rows = []
    reader = csv.DictReader(stream)
    for row in reader:
        try:
            rows.append({k.strip(): int(v) for k, v in row.items() if k and v.strip()})
        except (TypeError, ValueError):
            continue
    return rows


def summarize(rows):
    if not rows:
        print("没有有效数据")
        return
    first, last = rows[0], rows[-1]
    dt = max(last.get("t", 0) - first.get("t", 0), 1)
    tx = last.get("tx", 0) - first.get("tx", 0)
    rx = last.get("rx", 0) - first.get("rx", 0)
    retrans = last.get("retrans", 0) - first.get("retrans", 0)
    err = last.get("err", 0) - first.get("err", 0)
    tec = max(r.get("tec", 0) for r in rows)
    rec = max(r.get("rec", 0) for r in rows)

    lost = max(tx - rx, 0)
    rate = (retrans / tx * 100) if tx else 0.0

    print("=" * 46)
    print(f"  时长        : {dt} s")
    print(f"  发送 / 接收 : {tx} / {rx}")
    print(f"  丢帧        : {lost}")
    print(f"  重传        : {retrans}  ({rate:.3f}%)")
    print(f"  错误计数    : {err}")
    print(f"  TEC/REC 峰值: {tec} / {rec}")
    print(f"  平均速率    : {tx / dt:.1f} 帧/s")
    print("=" * 46)


def live(port, baud):
    try:
        import serial  # noqa: F401
    except ImportError:
        print("实时模式需要 pyserial：pip install pyserial")
        return

    import serial
    ser = serial.Serial(port, baud, timeout=1)
    print(f"打开 {port} @ {baud}，Ctrl+C 结束")
    rows = []
    try:
        while True:
            line = ser.readline().decode("utf-8", "ignore").strip()
            if not line or line.startswith("t,"):
                continue
            parts = line.split(",")
            if len(parts) != 7:
                continue
            try:
                row = {k: int(v) for k, v in zip(
                    ["t", "tx", "rx", "retrans", "err", "tec", "rec"], parts)}
            except ValueError:
                continue
            rows.append(row)
            print(f"\rt={row['t']}s tx={row['tx']} rx={row['rx']} "
                  f"retrans={row['retrans']} tec={row['tec']}", end="")
    except KeyboardInterrupt:
        print()
        summarize(rows)


def main():
    ap = argparse.ArgumentParser(description="CAN 网关统计工具")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p1 = sub.add_parser("stats", help="统计 CSV 文件")
    p1.add_argument("file")

    p2 = sub.add_parser("live", help="实时读取串口")
    p2.add_argument("port")
    p2.add_argument("--baud", type=int, default=115200)

    args = ap.parse_args()
    if args.cmd == "stats":
        with open(args.file, newline="", encoding="utf-8", errors="ignore") as f:
            summarize(read_rows(f))
    else:
        live(args.port, args.baud)


if __name__ == "__main__":
    main()
