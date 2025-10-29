#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <mosquitto.h>
#include "gpio_control.h"
#include <cjson/cJSON.h>
#include "sensor_ap3216c.h"

#define LED_GPIO    3
#define BEEP_GPIO   129

#define MQTT_HOST "192.168.1.82"
#define MQTT_PORT 1883
#define CMD_TOPIC "imx6ull/command"
#define STATUS_TOPIC "imx6ull/status"
#define SENSOR_TOPIC "imx6ull/sensor"
#define CLIENT_ID "imx6ull_integrated"

static volatile int running = 1;

// Message receive callback
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) 
{
    char *payload = (char*)msg->payload;
    char response[64];
    
    printf("get command: %s\n", payload);
    
    // Parse command
    if (strncmp(payload, "led:on", 6) == 0) 
    {
        led_control(1);  // low level = on
        strcpy(response, "led:on");
    }
    else if (strncmp(payload, "led:off", 7) == 0) 
    {
        led_control(0);  // high level = off
        strcpy(response, "led:off");
    }
    else if (strncmp(payload, "beep:on", 7) == 0) 
    {
        beep_control(1);
        strcpy(response, "beep:on");
    }
    else if (strncmp(payload, "beep:off", 8) == 0) 
    {
        beep_control(0);
        strcpy(response, "beep:off");
    }
    else 
    {
        strcpy(response, "unknown command");
    }
    
    // Publish status feedback
    mosquitto_publish(mosq, NULL, STATUS_TOPIC, strlen(response), response, 0, false);
}

// Callback for successful connection
void on_connect(struct mosquitto *mosq, void *obj, int rc) 
{
    printf("Connected! rc=%d\n", rc);
    
    // Subscribe to command topic after connection
    mosquitto_subscribe(mosq, NULL, CMD_TOPIC, 0);
    printf("Subscribe topic: %s\n", CMD_TOPIC);
}

void signal_handler(int sig) 
{
    printf("\nCaught signal %d, exiting...\n", sig);
    running = 0;
}


int main() 
{
    struct mosquitto *mosq;
    sensor_data_t sensor_data;
    int count = 0;
    
    // Initialize GPIO (direct register mapping)
    if (gpio_init() < 0) 
    {
        printf("GPIO init failed\n");
        return -1;
    }

    // 1. Initialize sensor
    if (ap3216c_init() < 0) 
    {
        fprintf(stderr, "init sensor failed\n");
        gpio_cleanup();
        return -1;
    }

    // Register signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("=== IMX6ULL MQTT -- Sensor + IOcontrol ===\n");
    
    // Initialize MQTT
    mosquitto_lib_init();
    mosq = mosquitto_new(CLIENT_ID, true, NULL);

    if (!mosq) 
    {
        fprintf(stderr, "Error: Failed to create mosquitto instance\n");
        ap3216c_close();
        gpio_cleanup();
        return -1;
    }
    
    // Set callbacks
    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);
    
    // Connect to broker
    if (mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60) != MOSQ_ERR_SUCCESS) 
    {
        fprintf(stderr, "Error: Could not connect to broker\n");
        ap3216c_close();
        gpio_cleanup();
        mosquitto_destroy(mosq);
        return -1;
    }
    
    // Start network loop
    printf("start network Loop...\n");
    mosquitto_loop_start(mosq);
    
    // Main loop: collect and publish data periodically
    printf("start capture data...\n");
    while (running) 
    {
        // Read sensor
        if (ap3216c_read(&sensor_data) == 0) 
        {
            // Create JSON object
            cJSON *root = cJSON_CreateObject();
            cJSON *data = cJSON_CreateObject();
            
            // Add device info
            cJSON_AddStringToObject(root, "device", CLIENT_ID);
            cJSON_AddNumberToObject(root, "timestamp", time(NULL));
            cJSON_AddNumberToObject(root, "count", count++);
            
            // Add sensor data
            cJSON_AddNumberToObject(data, "ir", sensor_data.ir);
            cJSON_AddNumberToObject(data, "als", sensor_data.als);
            cJSON_AddNumberToObject(data, "ps", sensor_data.ps);
            cJSON_AddItemToObject(root, "data", data);
            
            // Convert to JSON string
            char *json_str = cJSON_PrintUnformatted(root);
            
            // Publish message
            mosquitto_publish(mosq, NULL, SENSOR_TOPIC, strlen(json_str), json_str, 0, false);
            
            printf("→ %s\n", json_str);
            
            // Free memory
            // data is add into root do not free data!
            free(json_str);
            cJSON_Delete(root);
        } 
        else 
        {
            printf("Failed to read sensor\n");
        }
        
        sleep(1);  // Collect once per second
    }

    // Cleanup resources
    printf("\n cleanup res...\n");
    mosquitto_loop_stop(mosq, true);  // Stop network loop
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    ap3216c_close();
    gpio_cleanup();
    
    printf("Program exited\n");
    return 0;
}