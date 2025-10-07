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



## Baseline Day 1 — Automatic SDA fault injection (simple)

Focus today: build a clean, repeatable automatic I2C fault injection loop (SDA forced low for a fixed hold window) and emit structured logs ready for later detection & recovery phases.

What was added:
- Automatic periodic injector (no button needed). Cycle: every 5s start -> hold SDA low ~1s -> release.
- Clean state machine: IDLE -> INJECTING -> back to IDLE (scheduled next cycle).
- Baseline and end sensor sampling (AP3216C IR / PS / ALS) around each injection.
- Unified event queue + pump to avoid mixed printf timing.
- Stable CSV style log lines (key=value) with fixed schema.

Current log format (two lines per cycle):
```
af_csv version=1 mode=auto seq=N phase=inject  ts=... hold_ms=0   base_ir=.. base_ps=.. base_als=.. end_ir=.. end_ps=.. end_als=.. delta_ir=0 delta_ps=0 delta_als=0 drops=0
af_csv version=1 mode=auto seq=N phase=release ts=... hold_ms=1004 base_ir=.. base_ps=.. base_als=.. end_ir=.. end_ps=.. end_als=.. delta_ir=.. delta_ps=.. delta_als=.. drops=0
```

Observations:
- Cycle interval ≈ 6000 ms ( ~5000 ms gap + ~1000 ms hold ), jitter < ~30 ms (OK for polling loop).
- hold_ms drift (1000–1028) due to ms polling exit point — acceptable at this stage.
- Event queue not overflowing (drops=0).
- ALS currently reads 0 (may not be enabled yet) — to verify later.

Why this matters:
- Provides a deterministic “fault stimulus” baseline before adding bus stuck detection and recovery.
- The CSV schema is now stable; downstream scripts can parse without change when we add more phases (detect / recover).

Next planned steps (not done yet):
1. Bus stuck detection (monitor SDA/SCL + failed I2C probe)
2. Recovery pulses (toggle SCL up to 9 times + STOP + re-init)
3. Extend log schema: detection_latency, recovery_latency, pulses, success flag
4. Add canary globals + BSS range print for memory integrity
5. Error classification (NACK / timeout / arbitration lost)

Takeaway:
We now have a reproducible automatic fault injection loop with structured logs. This is the foundation for measuring detection and recovery effectiveness in the coming iterations.

