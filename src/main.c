#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <wiringPi.h>
#include <MQTTClient.h>

#define ADDRESS "tcp://mqtt.eclipseprojects.io:1883"
#define CLIENTID "MUHAHAH"
#define TOPIC "DDU4"
#define TIMEOUT 10000L

#define IN0 7
#define IN1 0
#define IN2 2
#define IN3 3

uint8_t circuitStatus = 0;
char statePayloadBuf[9];

MQTTClient client;
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
MQTTClient_message pubmsg = MQTTClient_message_initializer;
MQTTClient_deliveryToken token;

uint8_t getCircuitState() {
    uint8_t newState = digitalRead(IN0);
    newState |= digitalRead(IN1) << 1;
    newState |= digitalRead(IN2) << 2;
    newState |= digitalRead(IN3) << 3;

    return newState;
}

char* printState(uint8_t state) {
    int bitPos = 0;
    for (int i = 1 << 7; i != 0; i = i >> 1) {
        if ((state & i) > 0)
            statePayloadBuf[bitPos++] = '1';
        else
            statePayloadBuf[bitPos++] = '0';
    }
    statePayloadBuf[8] = '\0';
    return statePayloadBuf;
}

int main(int argc, char *argv[])
{
    wiringPiSetup();
    pinMode(IN0, INPUT);
    pullUpDnControl(IN0, PUD_DOWN);
    pinMode(IN1, INPUT);
    pinMode(IN2, INPUT);
    pinMode(IN3, INPUT);

    int rc;

    if ((rc = MQTTClient_create(&client, ADDRESS, CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
    {
         printf("Failed to create client, return code %d\n", rc);
         exit(EXIT_FAILURE);
    }

    conn_opts.keepAliveInterval = 20;
    // conn_opts.cleansession = 1;
    MQTTClient_SSLOptions sslOpts;
    sslOpts.verify = 0;
    // sslOpts.struct_version = 5;
    sslOpts.enableServerCertAuth = 0;
    // conn_opts.reliable = 1;
    // conn_opts.struct_version = 4;
    conn_opts.ssl = &sslOpts;
    printf("Connecting...\n");
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }

    for (;;) {
        uint8_t newCircuitState = getCircuitState();

        // Something changed... we should notify subscribers
        if (circuitStatus != newCircuitState) {
            circuitStatus = newCircuitState;
            printState(circuitStatus);
            printf("state: %s\n", statePayloadBuf);

            pubmsg.payload = statePayloadBuf;
            pubmsg.payloadlen = (int)strlen(statePayloadBuf);
            pubmsg.qos = 1;
            pubmsg.retained = 0;
            if ((rc = MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token)) != MQTTCLIENT_SUCCESS)
            {
                printf("Failed to publish message, return code %d\n", rc);
                exit(EXIT_FAILURE);
            }

            printf("Waiting for up to %d seconds for publication of %s\n"
                    "on topic %s for client with ClientID: %s\n",
                    (int)(TIMEOUT/1000), statePayloadBuf, TOPIC, CLIENTID);
            rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
            printf("Message with delivery token %d delivered\n", token);
        }
    }
    if ((rc = MQTTClient_disconnect(client, 10000)) != MQTTCLIENT_SUCCESS)
        printf("Failed to disconnect, return code %d\n", rc);
    MQTTClient_destroy(&client);

    return EXIT_SUCCESS;
}
