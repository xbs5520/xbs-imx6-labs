# RTSP Latency

Measure and improve end-to-end latency with a reproducible baseline on i.MX6UL. 

Start simple (HTTP MJPEG or RTP/UDP), then switch to RTSP for deeper controls. 

All measurements use OSD timestamps embedded in frames.

## Goals
- Mode : Bare‑metal
- Fault injection : Temporarily mux SDA pin to GPIO (open‑drain), pull it low to simulate a stuck slave; then switch back to I2C.
- Recovery:
  1) Detect bus busy (SDA low while SCL high)
  2) Quiesce controller (disable I2C, clear status)
  3) Manually toggle SCL (9 pulses) via GPIO to release SDA
  4) Re‑init I2C controller and probe target address
  5) If still stuck, optional device reset GPIO / power‑cycle

## Roadmap

Step 0: Bare‑metal detect + SCL pulse recovery with simple state machine; log timing and outcome.

Step 1: Integrate controller‑level recovery (module reset, status clear, timeout handling; handle stretch) and edge cases.

Step 2: Harden state machine (Idle → Detect → Quiesce → PulseSCL → CheckSDA → Reinit → Probe → Backoff/Fail), add retries and limits.

Step 3 (optional): Linux userspace tool using /dev/i2c‑X + sysfs GPIO to pulse SCL; or kernel integration via i2c‑gpio / bus recovery hooks.

Step 4: Stress: random fault injections at various phases (before START, mid‑byte, ACK), collect distributions and success rate.

## Metrics 

- Latency percentiles per fixed window (e.g., 60s): p50 (median), p95, p99 in ms.
- Drop rate (drop%) = dropped / expected frames.
- Optional: jitter (stddev), end-to-end budget breakdown when applicable.
- Ensure no idle/blank intervals are counted as 0 ms.

## Notes

- Mux carefully: When switching pins between I2C and GPIO, ensure lines are idle; add small delays for bus settle.
-  Measurement: Prefer a logic analyzer to validate actual SCL/SDA timing vs logs.
- Edge cases: Slave truly holds SDA low (internal fault); consider a bounded retry then device reset path.
- OS path: When porting to Linux, keep the same test cases and metrics to maintain comparability.
