# Stage 3: AWS IoT Cloud Integration

## Objective
Migrate local MQTT system to AWS IoT Core for cloud-based remote control and monitoring.

## Steps

### 1. AWS IoT Setup
- Created Thing: `imx6ull_device` in AWS IoT Console
- Generated X.509 certificates:
  - Device certificate (`imx_cert.pem`)
  - Private key (`imx_key.pem`)
  - Root CA (`imx_ca.pem`)
- Configured IoT Policy with permissions for pub/sub
- Retrieved endpoint: `a17rnczkphqzam-ats.iot.ca-central-1.amazonaws.com`

### 2. OpenSSL Upgrade (Critical Issue)
**Problem**: Mosquitto 2.0.18 + OpenSSL 1.1.1w failed with ALPN error  
**Root Cause**: AWS IoT requires ALPN extension (added in OpenSSL 3.0+)  
**Solution**: 
- Cross-compiled OpenSSL 3.0.15 for ARM
- Rebuilt Mosquitto 2.0.18 with new OpenSSL
- Verified ALPN support with `openssl s_client -alpn x-amzn-mqtt-ca`

### 3. Certificate Validation Issue
**Problem**: `certificate verify failed` error  
**Root Cause**: System time was January 1, 1970 (certificate not yet valid)  
**Solution**: Synchronized system time with NTP
```bash
ntpdate pool.ntp.org
hwclock -w  # Save to hardware clock
```

### 4. Code Implementation
- Modified connection parameters for TLS (port 8883)
- Configured certificate paths
- Updated topics for AWS IoT:
  - `imx6ull/sensor` - sensor data upload
  - `imx6ull/command` - remote control commands
  - `imx6ull/status` - device status

## Key Technical Points

### OpenSSL ALPN Support
```bash
# Verify ALPN is working:
openssl s_client -connect endpoint:8883 -alpn x-amzn-mqtt-ca

# Should see:
# ALPN protocol: x-amzn-mqtt-ca
```
- **ALPN (Application-Layer Protocol Negotiation)**: Required by AWS IoT
- **OpenSSL 3.0+**: First version with full ALPN support

### TLS Connection Configuration
```c
mosquitto_tls_set(mosq,
    "/home/root/certs/imx_ca.pem",      // Root CA
    NULL,                                // CA path (not used)
    "/home/root/certs/imx_cert.pem",    // Client cert
    "/home/root/certs/imx_key.pem",     // Private key
    NULL);                               // Password callback

mosquitto_tls_opts_set(mosq, 1, "tlsv1.2", NULL);
```
- **TLS 1.2**: Minimum version required by AWS IoT
- **Certificate verification**: Level 1 (verify server certificate)

### Time Synchronization
```bash
# Check current time
date

# If wrong (1970-01-01), sync with NTP:
ntpdate pool.ntp.org

# Save to hardware clock (persistent across reboots)
hwclock -w
```
- **Critical**: X.509 certificates have validity periods
- System time must be within certificate validity range

## Results
✅ Successfully connected to AWS IoT Core  
✅ Remote control from anywhere via AWS console  
✅ Real-time sensor data visible in cloud  
✅ TLS encryption working (port 8883)  
✅ Certificate authentication verified

## Challenges & Solutions

### Challenge 1: ALPN Protocol Error
```
Error: A TLS error occurred: The connection failed because ALPN negotiation failed
```
**Solution**: Upgraded OpenSSL 1.1.1w → 3.0.15, rebuilt Mosquitto

### Challenge 2: Certificate Verification Failed
```
Error: certificate verify failed (certificate is not yet valid)
```
**Solution**: Fixed system time using `ntpdate`, saved to RTC with `hwclock -w`

### Challenge 3: Cross-compilation Issues
**Problem**: OpenSSL 3.0.15 build errors with ARM toolchain  
**Solution**: Used correct configure options:
```bash
./Configure linux-generic32 \
    --prefix=/usr/local/arm/openssl-3.0.15-arm \
    --cross-compile-prefix=arm-linux-gnueabihf- \
    no-asm
```

## AWS IoT Testing

### Test from AWS IoT Console
1. Go to **Test** → **MQTT test client**
2. Subscribe to `imx6ull/sensor` - see real-time data
3. Publish to `imx6ull/command`:
```json
led:on
beep:off
```

### Monitor Device Shadow (Optional)
```bash
aws iot-data get-thing-shadow \
    --thing-name imx6ull_device \
    --output text shadow.json
```

## Output Example
```
=== IMX6ULL AWS IoT Client ===
Starting...
Initialize hardware
[GPIO] LED initialized (GPIO1_IO03)
[GPIO] BEEP initialized (GPIO5_IO01)
[Sensor] AP3216C initialized

Connecting to AWS IoT...
Endpoint: a17rnczkphqzam-ats.iot.ca-central-1.amazonaws.com:8883
Client ID: imx6ull_device

Connected to AWS IoT successfully!
Subscribed to: imx6ull/command

[Sensor] IR=120 ALS=450 PS=85
→ Published to imx6ull/sensor
← Command received: led:on
[GPIO] LED ON
```

## Architecture Evolution

### Stage 2: Local Network
```
PC ◄────LAN────► IMX6ULL
   (192.168.1.x)
```

### Stage 5: Cloud Integration
```
Phone/PC ◄──── Internet ────► AWS IoT Core
    │                              │
    │                              │ TLS 8883
    │                              │ (MQTT over TLS)
    │                              │
    └──────────────────────────────┼───► IMX6ULL
                                   │      + GPIO
                                   │      + Sensors
                                   │
                                   ▼
                        ┌────────────────────┐
                        │  AWS Services      │
                        │  - IoT Rules       │
                        │  - Lambda          │
                        │  - DynamoDB        │
                        │  - SNS/Email       │
                        └────────────────────┘
```

## Build Instructions

### 1. Build OpenSSL 3.0.15
```bash
cd openssl-3.0.15
./Configure linux-generic32 \
    --prefix=/usr/local/arm/openssl-3.0.15-arm \
    --cross-compile-prefix=arm-linux-gnueabihf- \
    no-asm
make -j4
make install
```

### 2. Build Mosquitto with OpenSSL 3.0.15
```bash
cd mosquitto-2.0.18
export PKG_CONFIG_PATH=/usr/local/arm/openssl-3.0.15-arm/lib/pkgconfig
make WITH_TLS=yes WITH_TLS_PSK=yes
make install DESTDIR=/path/to/rootfs
```

### 3. Compile Project
```bash
cd stage5_aws_iot
make
```

### 4. Deploy to Board
```bash
# Copy executable
scp mqtt_aws_iot root@192.168.1.100:/home/root/

# Copy certificates
scp cert/*.pem root@192.168.1.100:/home/root/certs/

# Run on board
ssh root@192.168.1.100
cd /home/root
./mqtt_aws_iot
```

## Security Considerations

### Certificate Storage
- ✅ Stored in `/home/root/certs/` with restricted permissions:
```bash
chmod 600 imx_key.pem    # Private key readable only by owner
chmod 644 imx_cert.pem   # Certificate can be world-readable
chmod 644 imx_ca.pem     # Root CA can be world-readable
```

### IoT Policy (JSON)
```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "iot:Connect",
      "Resource": "arn:aws:iot:ca-central-1:*:client/imx6ull_device"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Publish",
      "Resource": "arn:aws:iot:ca-central-1:*:topic/imx6ull/*"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Subscribe",
      "Resource": "arn:aws:iot:ca-central-1:*:topicfilter/imx6ull/*"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Receive",
      "Resource": "arn:aws:iot:ca-central-1:*:topic/imx6ull/*"
    }
  ]
}
```

