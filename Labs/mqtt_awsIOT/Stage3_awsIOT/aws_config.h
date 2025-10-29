#ifndef AWS_CONFIG_H
#define AWS_CONFIG_H

// AWS IoT Configuration
#define AWS_IOT_ENDPOINT "a17rnczkphqzam-ats.iot.ca-central-1.amazonaws.com"
#define AWS_IOT_PORT 8883
#define AWS_IOT_CLIENT_ID "imx6ull_device"

// Certificate file paths (using Amazon Root CA 1)
#define AWS_CERT_CA   "./cert/imx_ca.pem"
#define AWS_CERT_CRT  "./cert/imx_cert.pem"
#define AWS_CERT_KEY  "./cert/imx_key.pem"

// MQTT Topics
#define AWS_TOPIC_PUBLISH   "imx6ull/sensor"
#define AWS_TOPIC_SUBSCRIBE "imx6ull/command"
#define AWS_TOPIC_STATUS    "imx6ull/status"

#endif
