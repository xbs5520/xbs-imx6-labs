# Stage 1: MQTT Basic Connection Test

## Objective
Establish basic MQTT connection between IMX6ULL board and PC broker to verify network communication.

## step

### 1. Environment Setup
- Installed Mosquitto broker on PC (192.168.1.82:1883)
- Cross-compiled mosquitto library for ARM platform
- Configured network connection between board and PC

### 2. Implementation
- Created `mqtt_test.c` - basic MQTT client
- Implemented callback functions:
  - `on_connect()` - connection status
  - `on_publish()` - publish confirmation
  - `on_disconnect()` - disconnection handling
- Added signal handling (SIGINT/SIGTERM) for clean shutdown

### 3. Testing
- Successfully connected to broker
- Published test messages every 1 second
- Verified message delivery using `mosquitto_sub` on PC
- Tested graceful shutdown with Ctrl+C

## Key Technical Points

### MQTT Parameters
```c
mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60);
//                                              ^^
//                                              keepalive = 60s
```
- **keepalive**: 60 seconds heartbeat to maintain connection

```c
mosquitto_publish(mosq, NULL, MQTT_TOPIC, strlen(message), message, 0, false);
//                                                                   ^  ^^^^^
//                                                                   |  retain flag
//                                                                   QoS level (0)
```
- **QoS 0**: At most once delivery (fastest, no confirmation)
- **retain false**: Don't store message for new subscribers

### Cleanup Sequence
Critical order to avoid segfault:
1. `mosquitto_disconnect()` - close connection first
2. `mosquitto_loop_stop()` - stop network thread
3. `mosquitto_destroy()` - free client resources
4. `mosquitto_lib_cleanup()` - cleanup library

## Results
✅ Successfully established MQTT connection 
✅ Published messages reliably 
✅ Clean shutdown without errors 
✅ Ready for Stage 2 (GPIO/Sensor integration)

## Challenges & Solutions

**Challenge**: Initial connection timeout 
**Solution**: Verified PC firewall settings, ensured port 1883 is open

**Challenge**: Program hangs on exit 
**Solution**: Fixed cleanup order - disconnect before stopping loop

## Next Steps
- Stage 2: Integrate GPIO control (LED/BEEP)
- Add AP3216C sensor data publishing
- Implement command subscription (control hardware via MQTT)

## Build & Run
```bash
make
./mqtt_test

# On PC - subscribe to messages:
mosquitto_sub -h 192.168.1.82 -t imx6ull/test -v
```

## Output Example
```
=== IMX6ULL MQTT Test Client ===
Connecting to 192.168.1.82:1883
Connected to MQTT broker! Return code: 0
Connection successful!

Starting to publish messages...
→ Published: Hello from IMX6ULL! Count=0
Message published (mid=1)
→ Published: Hello from IMX6ULL! Count=1
Message published (mid=2)
...
```
