# P2_i2c_recovery Tools

- logger_schema.md: CSV schema and firmware print format.
- serial_logger.py: Reads firmware key=value logs from serial or a text file and writes CSV to data/.

Quick start:
- Option A (serial):
  ./serial_logger.py --port /dev/ttyUSB0 --baud 115200 --out ../data/run_$(date +%F_%H%M%S).csv
- Option B (from file):
  ./serial_logger.py --infile sample.log --out ../data/run_$(date +%F_%H%M%S).csv

Firmware should print per cycle, e.g.:
  iter=1 fault=sda_stuck td_ms=3.2 tr_ms=7.8 ok=1 attempts=1 pulses=9
