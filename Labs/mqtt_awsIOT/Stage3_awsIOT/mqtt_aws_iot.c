#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <mosquitto.h>
#include <cjson/cJSON.h>
#include "aws_config.h"
#include "gpio_control.h"
#include "sensor_ap3216c.h"

static volatile int running = 1;
static int message_count = 0;
static struct mosquitto *global_mosq = NULL;

void signal_handler(int sig) 
{
    printf("\nCaught signal %d, exiting...\n", sig);
    running = 0;
}

void on_connect(struct mosquitto *mosq, void *obj, int rc) 
{
    if (rc == 0) 
    {
        printf("Connected to AWS IoT successfully\n");
        
        // Subscribe to command topic
        int sub_rc = mosquitto_subscribe(mosq, NULL, AWS_TOPIC_SUBSCRIBE, 0);
        if (sub_rc == MOSQ_ERR_SUCCESS) 
        {
            printf("Subscribed to command topic\n");
        } else {
            fprintf(stderr, "Subscribe failed: %s\n", mosquitto_strerror(sub_rc));
        }
    } 
    else 
    {
        fprintf(stderr, "Connection failed: %s\n", mosquitto_strerror(rc));
        running = 0;
    }
}

void on_disconnect(struct mosquitto *mosq, void *obj, int rc) 
{
    if (rc == 0) 
    {
        printf("✓ Disconnected normally\n");
    } 
    else 
    {
        fprintf(stderr, "✗ Connection lost unexpectedly: %s\n", mosquitto_strerror(rc));
    }
}

void on_publish(struct mosquitto *mosq, void *obj, int mid) 
{
    // Publish successful, silent handling
}

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) 
{
    // Ensure payload is valid and in string format
    if (!msg->payload || msg->payloadlen == 0) 
    {
        printf("\n← Received empty command\n");
        return;
    }
    
    // Create NULL-terminated string
    char payload[256];
    int len = msg->payloadlen < 255 ? msg->payloadlen : 255;
    memcpy(payload, msg->payload, len);
    payload[len] = '\0';
    
    printf("\n← Received message: %s\n", payload);
    
    // Parse JSON
    cJSON *json = cJSON_Parse(payload);
    if (!json) 
    {
        printf("JSON parse failed\n");
        return;
    }
    
    // Extract command field
    cJSON *cmd = cJSON_GetObjectItem(json, "command");
    if (!cmd || !cJSON_IsString(cmd)) 
    {
        printf("Command field not found\n");
        cJSON_Delete(json);
        return;
    }
    
    const char *command = cmd->valuestring;
    printf("→ Executing command: %s\n", command);
    
    // Handle LED commands
    if (strcmp(command, "led_on") == 0 || strcmp(command, "led:on") == 0) 
    {
        led_control(1);
        printf("LED turned on\n");
    } 
    else if (strcmp(command, "led_off") == 0 || strcmp(command, "led:off") == 0) 
    {
        led_control(0);
        printf("LED turned off\n");
    }
    // Handle BEEP commands
    else if (strcmp(command, "beep_on") == 0 || strcmp(command, "beep:on") == 0) 
    {
        beep_control(1);
        printf("Buzzer turned on\n");
    }
    else if (strcmp(command, "beep_off") == 0 || strcmp(command, "beep:off") == 0) 
    {
        beep_control(0);
        printf("Buzzer turned off\n");
    }
    else 
    {
        printf("Unknown command: %s\n", command);
        cJSON_Delete(json);
        return;
    }
    
    // Send status feedback
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddStringToObject(response, "command", command);
    cJSON_AddNumberToObject(response, "timestamp", time(NULL));
    
    char *response_str = cJSON_PrintUnformatted(response);
    mosquitto_publish(mosq, NULL, AWS_TOPIC_STATUS, strlen(response_str), response_str, 0, false);
    printf("Status reply sent\n");
    
    free(response_str);
    cJSON_Delete(response);
    cJSON_Delete(json);
}

void on_log(struct mosquitto *mosq, void *obj, int level, const char *str) 
{
    // Only show warnings and errors
    if (level == MOSQ_LOG_WARNING) 
    {
        printf("[Warning] %s\n", str);
    } 
    else if (level == MOSQ_LOG_ERR) 
    {
        printf("[Error] %s\n", str);
    }
}

// Data publishing thread (independent from mosquitto network loop)
void* publish_thread(void *arg) 
{
    struct mosquitto *mosq = (struct mosquitto *)arg;
    sensor_data_t sensor_data;
    printf("\nData publishing thread started\n");
    sleep(2);  // Wait for connection stable
    
    while (running) 
    {
        // Create JSON data
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "device", AWS_IOT_CLIENT_ID);
        cJSON_AddNumberToObject(root, "timestamp", time(NULL));
        cJSON_AddNumberToObject(root, "count", ++message_count);
        
        // Read sensor data
        if (ap3216c_read(&sensor_data) == 0) 
        {
            cJSON *data = cJSON_CreateObject();
            cJSON_AddNumberToObject(data, "als", sensor_data.als);  // Ambient light
            cJSON_AddNumberToObject(data, "ps", sensor_data.ps);    // Proximity
            cJSON_AddNumberToObject(data, "ir", sensor_data.ir);    // Infrared
            
            cJSON_AddItemToObject(root, "data", data);
        } else 
        {
            // Sensor read failed, use dummy data
            cJSON *data = cJSON_CreateObject();
            cJSON_AddNumberToObject(data, "als", 0);
            cJSON_AddNumberToObject(data, "ps", 0);
            cJSON_AddNumberToObject(data, "ir", 0);
            cJSON_AddItemToObject(root, "data", data);
            printf("Sensor read failed\n");
        }
        
        // Convert to string
        char *json_str = cJSON_PrintUnformatted(root);
        
        // Publish to AWS IoT
        int rc = mosquitto_publish(mosq, NULL, AWS_TOPIC_PUBLISH,
                                  strlen(json_str), json_str, 0, false);
        
        if (rc == MOSQ_ERR_SUCCESS) 
        {
            printf("→ [%d] %s\n", message_count, json_str);
        } 
        else 
        {
            fprintf(stderr, "✗ Publish failed: %s (rc=%d)\n", mosquitto_strerror(rc), rc);
        }
        
        // Cleanup
        free(json_str);
        cJSON_Delete(root);
 
        // Wait 20 seconds
        sleep(10);
    }
    
    printf("Data publishing thread exited\n");
    return NULL;
}

int main() 
{
    struct mosquitto *mosq;
    int rc;
    pthread_t pub_thread;
    
    printf("=== IMX6ULL AWS IoT Client ===\n");
    printf("Starting...\n\n");
    
    // Initialize hardware
    if (gpio_init() != 0) 
    {
        fprintf(stderr, "GPIO initialization failed\n");
        return -1;
    }
    printf("GPIO initialized successfully\n");
    
    if (ap3216c_init() != 0) 
    {
        fprintf(stderr, "AP3216C sensor initialization failed\n");
        gpio_cleanup();
        return -1;
    }
    printf("AP3216C sensor initialized successfully\n\n");
    
    // Register signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize mosquitto library
    mosquitto_lib_init();
    
    // Create client instance
    mosq = mosquitto_new(AWS_IOT_CLIENT_ID, true, NULL);
    if (!mosq) 
    {
        fprintf(stderr, "Failed to create client\n");
        return -1;
    }
    global_mosq = mosq;
    
    // Set protocol version to MQTT 3.1.1
    mosquitto_int_option(mosq, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V311);
    
    // Set callback functions
    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_disconnect_callback_set(mosq, on_disconnect);
    mosquitto_publish_callback_set(mosq, on_publish);
    mosquitto_message_callback_set(mosq, on_message);
    mosquitto_log_callback_set(mosq, on_log);
    
    // Configure TLS/SSL
    rc = mosquitto_tls_set(mosq, AWS_CERT_CA, NULL, AWS_CERT_CRT, AWS_CERT_KEY, NULL);
    
    if (rc != MOSQ_ERR_SUCCESS) 
    {
        fprintf(stderr, "TLS configuration failed: %s\n", mosquitto_strerror(rc));
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return -1;
    }
    
    // Connect to AWS IoT
    printf("Connecting to AWS IoT...\n");
    rc = mosquitto_connect(mosq, AWS_IOT_ENDPOINT, AWS_IOT_PORT, 60);
    if (rc != MOSQ_ERR_SUCCESS) 
    {
        fprintf(stderr, "Connection failed: %s\n", mosquitto_strerror(rc));
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return -1;
    }
    
    // Create data publishing thread
    pthread_create(&pub_thread, NULL, publish_thread, mosq);
    
    // Main thread: network loop
    while (running) 
    {
        rc = mosquitto_loop(mosq, 1000, 1);
        if (rc != MOSQ_ERR_SUCCESS) 
        {
            fprintf(stderr, "Network error: %s\n", mosquitto_strerror(rc));
            if (rc == MOSQ_ERR_NO_CONN || rc == MOSQ_ERR_CONN_LOST) 
            {
                printf("Reconnecting...\n");
                mosquitto_reconnect(mosq);
                sleep(3);
            } 
            else 
            {
                break;
            }
        }
    }

    // Wait for publishing thread to finish
    pthread_join(pub_thread, NULL);
    
    // Cleanup resources
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    
    // Cleanup hardware
    gpio_cleanup();
    ap3216c_close();
    
    printf("\nProgram exited\n");
    return 0;
}
