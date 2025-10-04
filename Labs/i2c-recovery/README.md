# I2C Recovery

Detect and recover from I2C bus lockups (SDA held low, clock stretch abuse) on i.MX6UL.

## Goals
- Detect stuck bus conditions reliably
- Implement recovery: manual SCL toggling, re‑init, device reset strategies
- Provide test harness and pass/fail criteria

## Plan
- Fault injection (hold SDA low, stretch SCL)
- Recovery state machine
- Logs and metrics (recovery time, success rate)
