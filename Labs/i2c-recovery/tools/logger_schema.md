# I2C Recovery Baseline: Logging & CSV Schema

We’ll capture one line per injection/recovery cycle. Fields:

CSV header:

iter, ts_host_ms, fault_type, td_ms, tr_ms, ok, attempts, scl_pulses, notes

- iter: monotonically increasing iteration id (start from 1)
- ts_host_ms: host timestamp when line received (ms since epoch)
- fault_type: none|sda_stuck|stretch|mid_byte|ack_phase
- td_ms: detection time (fault injected -> detection)
- tr_ms: recovery time (start recovery -> first success)
- ok: 1 success, 0 failure within limits
- attempts: number of recovery attempts performed
- scl_pulses: total SCL pulses sent during recovery
- notes: free text for anomalies

Firmware print (single line, space or comma delimited):

iter=42 fault=sda_stuck td_ms=3.2 tr_ms=7.8 ok=1 attempts=1 pulses=9

Host will append ts_host_ms and notes if any.
