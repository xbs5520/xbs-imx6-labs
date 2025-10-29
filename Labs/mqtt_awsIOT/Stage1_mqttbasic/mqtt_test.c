
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mosquitto.h>
#include <signal.h>

#define MQTT_HOST "192.168.1.82"  // PC IP address
#define MQTT_PORT 1883
#define MQTT_TOPIC "imx6ull/test"
#define CLIENT_ID "imx6ull_board"

static volatile int running = 1;

void on_connect(struct mosquitto *mosq, void *obj, int rc) 
{
    printf("Connected to MQTT broker! Return code: %d\n", rc);
    if (rc == 0) {
        printf("Connection successful!\n");
    } else {
        printf("Connection failed!\n");
    }
}

void on_publish(struct mosquitto *mosq, void *obj, int mid) 
{
    printf("Message published (mid=%d)\n", mid);
}

void on_disconnect(struct mosquitto *mosq, void *obj, int rc) 
{
    printf("Disconnected from broker (rc=%d)\n", rc);
}

void signal_handler(int sig) {
    printf("\nCaught signal %d, exiting...\n", sig);
    running = 0;
}

int main() 
{
    struct mosquitto *mosq;
    int rc;
    int count = 0;
    char message[128];
    
    printf("=== IMX6ULL MQTT Test Client ===\n");
    printf("Connecting to %s:%d\n", MQTT_HOST, MQTT_PORT);
    
    // Initialize mosquitto library
    mosquitto_lib_init();

    // handle kill && ctrl + c
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create mosquitto client instance
    mosq = mosquitto_new(CLIENT_ID, true, NULL);
    if (!mosq) 
    {
        fprintf(stderr, "Error: Failed to create mosquitto instance\n");
        return 1;
    }
    
    // Set callback functions
    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_publish_callback_set(mosq, on_publish);
    mosquitto_disconnect_callback_set(mosq, on_disconnect);
    
    // Connect to broker    60-keepAlive
    rc = mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60);
    if (rc != MOSQ_ERR_SUCCESS) 
    {
        fprintf(stderr, "Error: Could not connect to broker (rc=%d)\n", rc);
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return 1;
    }
    
    // Start network loop (non-blocking)
    mosquitto_loop_start(mosq);
    
    // Send test messages
    printf("\nStarting to publish messages...\n");
    while (running) 
    {
        snprintf(message, sizeof(message), "Hello from IMX6ULL! Count=%d", count);
        
        rc = mosquitto_publish(mosq, NULL, MQTT_TOPIC, strlen(message), message, 0, false);
        
        if (rc != MOSQ_ERR_SUCCESS) 
        {
            fprintf(stderr, "Error publishing: %s\n", 
                    mosquitto_strerror(rc));
        } 
        else 
        {
            printf("→ Published: %s\n", message);
        }
        
        count++;
        sleep(1);
    }
    
    printf("\nTest complete! Cleaning up...\n");
    
    // Cleanup (disconnect first, then stop loop)
    printf("→ Disconnecting...\n");
    mosquitto_disconnect(mosq);
    
    printf("→ Stopping loop...\n");
    mosquitto_loop_stop(mosq, true);  // true = force stop
    
    printf("→ Destroying client...\n");
    mosquitto_destroy(mosq);
    
    printf("→ Cleaning up library...\n");
    mosquitto_lib_cleanup();
    
    printf("Done!\n");
    return 0;
}
