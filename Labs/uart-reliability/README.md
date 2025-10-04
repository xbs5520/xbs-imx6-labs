# UART Reliability

Zero‑loss UART RX pipeline on i.MX6UL (Cortex‑A7), evolving from Blocking → IRQ (ring buffer) → DMA double buffer (A/B). Comparable metrics at every step.

## Goals
- Zero data loss under sustained bursts
- Quantify CPU busy% (PMU cycle‑based)
- Keep metrics comparable across iterations

## Roadmap
1. Baseline: Blocking read + opportunistic drain + pre‑sync
2. 1.1: Improved busy% using PMU cycle counter
3. IRQ: RX interrupt + ring buffer
4. DMA: A/B double buffer with half/full callbacks

## Metrics
- bytes, lost, overruns, max_burst
- busy% = (cyc_total − cyc_wait) / cyc_total × 100

## Sender
- Host python: send repeating 0..255 sequence; wait for READY

## Notes
- Vendor headers and full BSP are excluded; only minimal code snippets and docs are kept for clarity.
