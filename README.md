# xbs-imx6-labs

Bare-metal and low-level firmware labs on i.MX6UL (Cortex‑A7). This repo hosts small, focused projects with measurable outcomes and clear evolution steps.

- Projects
  - UART Reliability (blocking → IRQ → DMA A/B) with zero‑loss RX and PMU‑based busy%
  - RTSP Latency (end‑to‑end measurement and jitter budget)
  - I2C Recovery (bus hang detection and clock‑stretch recovery)

## Structure
```
projects/
  uart-reliability/
  rtsp-latency/
  i2c-recovery/
docs/
```

## Highlights
- i.MX6UL / Cortex‑A7 focus, but design patterns are portable
- Staged evolution per project; each stage has success criteria and metrics
- Minimal, readable code; vendor blobs excluded

## Getting started
- Each project folder has its own README with goals, milestones, and how to run/measure.
- This repo is a showcase: source snippets and docs only; full BSP/toolchains are intentionally omitted.

## License
MIT (unless stated otherwise per subfolder)
