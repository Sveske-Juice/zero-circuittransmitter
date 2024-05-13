#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <wiringPi.h>
#include <MQTTClient.h>

#define ADDRESS "tcp://mqtt.eclipseprojects.io:1883"
#define CLIENTID "KMG-DDU4-DigiLogi"
#define INIT_TOPIC "DDU4/init"
#define EVAL_ROW_TOPIC "DDU4/evalrow"
#define STATE_TOPIC "DDU4/state"
#define QOS 1
#define TIMEOUT 10000L

#define IN0 7
#define IN1 0
#define IN2 2
#define IN3 3

char* getLocalIPv4();

uint8_t circuitStatus = 0;
char statePayloadBuf[9];

MQTTClient_deliveryToken deliveredtoken;
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

void delivered(void *context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: %.*s\n", message->payloadlen, (char*)message->payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
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

    if ((rc = MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to set callbacks, return code %d\n", rc);
        rc = EXIT_FAILURE;
	MQTTClient_destroy(&client);
	return rc;
    }
 
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    printf("Connecting...\n");
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        rc = EXIT_FAILURE;
	MQTTClient_destroy(&client);
	return rc;
    }
    printf("Subscribing to topic %s\nfor client %s using QoS%d\n\n"
           , EVAL_ROW_TOPIC, CLIENTID, QOS);
    if ((rc = MQTTClient_subscribe(client, EVAL_ROW_TOPIC, QOS)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to subscribe, return code %d\n", rc);
        rc = EXIT_FAILURE;
    }

    char *ip = getLocalIPv4();
    pubmsg.payload = ip;
    pubmsg.payloadlen = (int)strlen(ip);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    deliveredtoken = 0;

    if ((rc = MQTTClient_publishMessage(client, INIT_TOPIC, &pubmsg, &token)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to publish message, return code %d\n", rc);
        rc = EXIT_FAILURE;
    }
    else
    {
        printf("Waiting for publication of %s\n"
            "on topic %s for client with ClientID: %s\n",
            ip, INIT_TOPIC, CLIENTID);
        while (deliveredtoken != token)
        {
		usleep(10000L);
        }
    }
 

    // Main loop
    do
    {
	    uint8_t newCircuitState = getCircuitState();
	    // Something changed... we should notify subscribers
	    if (circuitStatus != newCircuitState) {
		    circuitStatus = newCircuitState;
		    printState(circuitStatus);
		    printf("state: %s\n", statePayloadBuf);

		    pubmsg.payload = statePayloadBuf;
		    pubmsg.payloadlen = (int)strlen(statePayloadBuf);
		    pubmsg.qos = QOS;
		    pubmsg.retained = 0;
		    if ((rc = MQTTClient_publishMessage(client, STATE_TOPIC, &pubmsg, &token)) != MQTTCLIENT_SUCCESS)
		    {
			    printf("Failed to publish message, return code %d\n", rc);
			    exit(EXIT_FAILURE);
		    }
		    else
		    {
			    printf("Waiting for publication of %s\n"
					    "on topic %s for client with ClientID: %s\n",
					    ip, STATE_TOPIC, CLIENTID);
			    while (deliveredtoken != token)
			    {
				    usleep(10000L);
			    }
		    }

	    }
    } while (1);

    if ((rc = MQTTClient_unsubscribe(client, EVAL_ROW_TOPIC)) != MQTTCLIENT_SUCCESS)
    {
	    printf("Failed to unsubscribe, return code %d\n", rc);
	    rc = EXIT_FAILURE;
    }

    if ((rc = MQTTClient_disconnect(client, 10000)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to disconnect, return code %d\n", rc);
        rc = EXIT_FAILURE;
    }

    return rc;
}

char* getLocalIPv4() {
    struct ifaddrs *addrs, *tmp;
    char* ipv4_address = NULL;

    // Get list of interface addresses
    if (getifaddrs(&addrs) == -1) {
        perror("getifaddrs");
        return NULL;
    }

    // Iterate through the list of addresses
    for (tmp = addrs; tmp != NULL; tmp = tmp->ifa_next) {
        // Check if it's an IPv4 address and not loopback
        if (tmp->ifa_addr != NULL && tmp->ifa_addr->sa_family == AF_INET && strcmp(tmp->ifa_name, "lo") != 0) {
            struct sockaddr_in *pAddr = (struct sockaddr_in *)tmp->ifa_addr;
            ipv4_address = strdup(inet_ntoa(pAddr->sin_addr));
            break; // Only take the first non-loopback IPv4 address
        }
    }

    // Free memory
    freeifaddrs(addrs);

    return ipv4_address;
}
