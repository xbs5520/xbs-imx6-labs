# Stage 2: Local MQTT with GPIO & Sensor Control

## Objective
Integrate hardware control (LED/BEEP) and sensor data (AP3216C) with MQTT for local IoT system.

## Step

### 1. Hardware Integration
- Mapped GPIO registers using `mmap()` for direct hardware control
  - GPIO1_IO03 (LED) - active low
  - GPIO5_IO01 (BEEP) - active low
- Implemented I2C communication with AP3216C sensor
  - Ambient Light Sensor (ALS)
  - Proximity Sensor (PS)
  - Infrared (IR) detection

### 2. MQTT Command System
- Subscribed to `imx6ull/command` topic
- Implemented command parser:
  - `led:on` / `led:off` - LED control
  - `beep:on` / `beep:off` - BEEP control
- Published sensor data to `imx6ull/sensor` every 2 seconds

### 3. Data Format (JSON)
```json
{
  "timestamp": 1234567890,
  "ir": 125,
  "als": 456,
  "ps": 89
}
```

## Key Technical Points

### GPIO Memory Mapping
```c
#define GPIO_SIZE 0x1000  // 4KB page size

gpio1_base = mmap(NULL, GPIO_SIZE, PROT_READ | PROT_WRITE,
                  MAP_SHARED, fd, GPIO1_BASE);
```
- **0x1000 (4KB)**: Standard memory page size
- Maps entire GPIO register block at once

### I2C Read Protocol
```c
// Step 1: Write register address
write(i2c_fd, &reg_addr, 1);

// Step 2: Read data bytes
read(i2c_fd, buffer, length);
```
- **Write-then-read**: Standard I2C register access pattern

### AP3216C Configuration
```c
ap3216c_write_reg(SYSTEM_CONFIG, 0x04);  // Soft reset
usleep(50000);                           // Wait 50ms
ap3216c_write_reg(SYSTEM_CONFIG, 0x03);  // Enable ALS+PS+IR
usleep(150000);                          // Wait 150ms
```
- **0x04**: Software reset bit
- **0x03**: Enable all three sensors simultaneously

### Sensor Data Parsing
```c
// IR: 14-bit (bits 0-13 of buf[0:1])
ir = ((buf[1] & 0x3F) << 8) | buf[0];

// ALS: 16-bit full scale
als = (buf[3] << 8) | buf[2];

// PS: 12-bit (bits 0-11 of buf[4:5])
ps = ((buf[5] & 0x0F) << 8) | buf[4];
```
- Uses bit masking to extract multi-byte values

## Results
✅ Successfully controlled GPIO via MQTT commands  
✅ Published real-time sensor data every 2 seconds  
✅ Bidirectional MQTT communication working  
✅ JSON format for easy PC-side parsing

## Challenges & Solutions

**Challenge**: GPIO not responding 
**Solution**: Fixed address calculation - must use base + offset for each register

**Challenge**: AP3216C returns all zeros 
**Solution**: Added proper initialization sequence with delays after reset

**Challenge**: I2C read timeout 
**Solution**: Implemented write-then-read protocol correctly (separate operations)

**Challenge**: Sensor data parsing errors 
**Solution**: Used proper bit masking for 12/14/16-bit values

## Testing Commands

### Control Hardware (from PC)
```bash
# LED control
mosquitto_pub -h 192.168.1.82 -t imx6ull/command -m "led:on"
mosquitto_pub -h 192.168.1.82 -t imx6ull/command -m "led:off"

# BEEP control
mosquitto_pub -h 192.168.1.82 -t imx6ull/command -m "beep:on"
mosquitto_pub -h 192.168.1.82 -t imx6ull/command -m "beep:off"
```

### Monitor Sensor Data (on PC)
```bash
mosquitto_sub -h 192.168.1.82 -t imx6ull/sensor -v
```

## Output Example
```
=== IMX6ULL MQTT Control Client ===
[GPIO] LED initialized
[GPIO] BEEP initialized
[Sensor] AP3216C initialized successfully
Connected to MQTT broker successfully!
Subscribed to: imx6ull/command

→ Sensor data published: {"timestamp":1234567890,"ir":125,"als":456,"ps":89}
← Received command: led:on
[CMD] LED turned ON
→ Sensor data published: {"timestamp":1234567892,"ir":130,"als":460,"ps":85}
← Received command: beep:on
[CMD] BEEP turned ON
```

## Architecture
```
┌──────────────┐          MQTT            ┌──────────────┐
│   PC         │ ◄─────────────────────►  │  IMX6ULL     │
│              │   Command Topic          │              │
│ mosquitto_   │   imx6ull/command        │  GPIO +      │
│ pub/sub      │                          │  I2C         │
│              │   Sensor Topic           │  Sensor      │
│              │   imx6ull/sensor         │              │
└──────────────┘                          └──────────────┘
                                                  │
                                                  ▼
                                          ┌───────────────┐
                                          │  LED   BEEP   │
                                          │  AP3216C      │
                                          └───────────────┘
```

## Next Steps
- Stage 3: Migrate to TLS-enabled Mosquitto && Connect to AWS IoT Cloud