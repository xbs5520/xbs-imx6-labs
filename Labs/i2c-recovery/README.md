# I2C Recovery

Recover from I2C bus lockups (SDA held low, abusive 滥用 clock stretching) on i.MX6UL with a measurable, repeatable baseline. 

Start on bare‑metal for full pin/control access, then optionally port to Linux.

## Goals
- Detect stuck bus conditions reliably
- Implement recovery: manual SCL toggling, re‑init, device reset strategies
- Provide test harness and pass/fail criteria

## Plan
- Fault injection (hold SDA low, stretch SCL)
- Recovery state machine
- Logs and metrics (recovery time, success rate)

## Baseline 00 — No‑fault logging sanity

目标：先把“测量链路”打通（固件→串口→CSV），不注入故障，验证日志/字段/时间戳都正常。

- 固件打印格式（每循环一行，key=value）：
  - Example: `iter=1 fault=none td_ms=0 tr_ms=0 ok=1 attempts=0 pulses=0`
  - 字段含义参见 `tools/logger_schema.md`
- 启动主机日志器（需要 pyserial，或用 --infile 先离线测试）：
  - `tools/serial_logger.py --port /dev/ttyUSB0 --baud 115200 --out data/run_YYYYmmdd_HHMMSS.csv`
- 运行 1–2 分钟，检查 CSV：
  - 行在增长；fault_type 应为 none；td_ms/tr_ms 为 0；ok=1
- 产出：保存该 CSV 为 baseline（无故障），后续对比用。

资源：
- 工具说明：`P2_i2c_recovery/tools/README.md`
- CSV/字段：`P2_i2c_recovery/tools/logger_schema.md`
