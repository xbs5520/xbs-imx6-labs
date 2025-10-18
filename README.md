# xbs-imx6-labs

Bare-metal and low-level firmware labs on i.MX6UL (Cortex‑A7). This repo hosts small, focused projects with measurable outcomes and clear evolution steps.

- Projects
  - UART optimization (blocking → IRQ → DMA A/B) with zero‑loss RX and PMU‑based busy%
  - RTOS migration

## Structure
```
labs/
  uart_optimization/
  freertos_migration/
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
