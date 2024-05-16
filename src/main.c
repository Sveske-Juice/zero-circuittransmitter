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
#define INIT_TOPIC "DDU4/DigitalLogik/init"
#define EVAL_ROW_TOPIC "DDU4/DigitalLogik/testrequest"
#define EVAL_RES_TOPIC "DDU4/DigitalLogik/testresult"
#define STATE_TOPIC "DDU4/DigitalLogik/state"
#define QOS 1
#define TIMEOUT 10000L

#define IN0 7
#define IN1 0
#define IN2 2
#define IN3 3
#define OUT0 1
#define OUT1 4
#define OUT2 28
#define OUT3 29

char* getLocalIPv4();

uint8_t circuitStatus = 0;
char statePayloadBuf[18];

MQTTClient_deliveryToken deliveredtoken;
MQTTClient client;
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
MQTTClient_message pubmsg = MQTTClient_message_initializer;
MQTTClient_deliveryToken token;

int rc;

uint8_t getCircuitState() {
    uint8_t newState = digitalRead(IN0);
    newState |= digitalRead(IN1) << 1;
    newState |= digitalRead(IN2) << 2;
    newState |= digitalRead(IN3) << 3;
    newState |= digitalRead(OUT0) << 4;
    newState |= digitalRead(OUT1) << 5;
    newState |= digitalRead(OUT2) << 6;
    newState |= digitalRead(OUT3) << 7;

    return newState;
}

char* printState(uint8_t state) {
    statePayloadBuf[0] = 's';
    snprintf(statePayloadBuf, 5, "s%d\n", state);
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
	char *payload = message->payload;
	switch (payload[0]){
		// test request
		case 'r':
			uint8_t tableRows[16];
			char currentStateBuf[4];
			currentStateBuf[3] = '\0';
			int rowIdx = 0;
			int currentStateIdx = 0;
			for (int i = 1; i < message->payloadlen; i++) {
				if (payload[i] == ' ') {
					printf("%s\n", currentStateBuf);
					tableRows[rowIdx++] = atoi(currentStateBuf);

					currentStateIdx = 0;
					currentStateBuf[0] = 0;
					currentStateBuf[1] = 0;
					currentStateBuf[2] = 0;
					continue;
				}
				currentStateBuf[currentStateIdx++] = payload[i];
			}
			tableRows[rowIdx++] = atoi(currentStateBuf);
			for (int i = 0; i < rowIdx; i++) {
				printf("%d ", tableRows[i]);
				// Perform test

				usleep(1000000L);

				// Return result
				char *payload = "SUCCESS";
				pubmsg.payload = payload;
				pubmsg.payloadlen = (int)strlen(payload);
				pubmsg.qos = QOS;
				pubmsg.retained = 0;
				deliveredtoken = 0;
				if ( MQTTClient_publishMessage(client, EVAL_RES_TOPIC, &pubmsg, &token) != MQTTCLIENT_SUCCESS)
				{
					printf("Failed to publish message, return code %d\n", rc);
					rc = EXIT_FAILURE;
					return rc;
				}
				else
				{
					while (deliveredtoken != token)
					{
						usleep(10000L);
					}
				}
			}
			printf("\n");
			break;
	}
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

    pinMode(OUT0, INPUT);
    pinMode(OUT1, INPUT);
    pinMode(OUT2, INPUT);
    pinMode(OUT3, INPUT);


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
