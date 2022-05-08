// Demo3.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <iostream>
#include "include\mosquitto.h"
#include "Demo3.h"
#include <string>
#include <vector>
#include <thread>

#define SDK_Version "1.0.0"

struct mosquitto *mosq;
int rc; 

char *m_cafile;
char* m_Certfile;
char* m_Keyfile;
char* MQTT_TOPIC;

//#define MQTT_HOSTNAME "mqtt.sandbox.thingplus.net"
//#define MQTT_PORT 8883
//#define MQTT_CLIENTID "cc7a10000002"
//#define MQTT_USERNAME "cc7a10000002"
//#define MQTT_PASSWORD "xuS3BlVVE8Vn2C410y1fs_eOlDs="
//#define MQTT_TOPIC "v/a/g/cc7a10000002/s/cc7a10000002-number-1"

char text[100] = "[1543388495584,22]";
static bool connected = true;
static bool disconnect_sent = false;
static int mid_sent = 0;
static int qos = 0;
static int retain = 0;


std::thread threadObj;
std::vector<std::string> recv_data;
char* GetVersion()
{
	return (char*)SDK_Version;
}



void thread_function()
{
	std::cout << "thread_function Executing\n";
	do {
		rc = mosquitto_loop(mosq, -1, 1);
	} while (rc == MOSQ_ERR_SUCCESS && connected);
	
}

//________________________________subs_________________________________________________

/* Callback called when the client receives a CONNACK message from the broker. */
void on_connect(struct mosquitto *mosq, void *obj, int reason_code)
{
	int rc;
	/* Print out the connection result. mosquitto_connack_string() produces an
	* appropriate string for MQTT v3.x clients, the equivalent for MQTT v5.0
	* clients is mosquitto_reason_string().
	*/
	printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
	if (reason_code != 0) {
		/* If the connection fails for any reason, we don't want to keep on
		* retrying in this example, so disconnect. Without this, the client
		* will attempt to reconnect. */
		mosquitto_disconnect(mosq);
	}

	/* Making subscriptions in the on_connect() callback means that if the
	* connection drops and is automatically resumed by the client, then the
	* subscriptions will be recreated when the client reconnects. */
	rc = mosquitto_subscribe(mosq, NULL, "example/temperature", 1);
	if (rc != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "Error subscribing: %s\n", mosquitto_strerror(rc));
		/* We might as well disconnect if we were unable to subscribe */
		mosquitto_disconnect(mosq);
	}
}


/* Callback called when the broker sends a SUBACK in response to a SUBSCRIBE. */
void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	int i;
	bool have_subscription = false;

	/* In this example we only subscribe to a single topic at once, but a
	* SUBSCRIBE can contain many topics at once, so this is one way to check
	* them all. */
	for (i = 0; i<qos_count; i++) {
		printf("on_subscribe: %d:granted qos = %d\n", i, granted_qos[i]);
		if (granted_qos[i] <= 2) {
			have_subscription = true;
		}
	}
	if (have_subscription == false) {
		/* The broker rejected all of our subscriptions, we know we only sent
		* the one SUBSCRIBE, so there is no point remaining connected. */
		fprintf(stderr, "Error: All subscriptions rejected.\n");
		mosquitto_disconnect(mosq);
	}
}


/* Callback called when the client receives a message. */
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
	/* This blindly prints the payload, but the payload can be anything so take care. */
	printf("%s %d %s\n", msg->topic, msg->qos, (char *)msg->payload);

	recv_data.emplace_back(*(std::string*)msg->payload);
}

//______________________________subs_____________________________________________-

//__________________________________push_____________________________________________

void my_connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	int rc = MOSQ_ERR_SUCCESS;

	if (!result) {
		printf("Sending message...\n");
		rc = mosquitto_publish(mosq, &mid_sent, MQTT_TOPIC, strlen(text), text, qos, retain);
		if (rc) {
			switch (rc) {
			case MOSQ_ERR_INVAL:
				fprintf(stderr, "Error: Invalid input. Does your topic contain '+' or '#'?\n");
				break;
			case MOSQ_ERR_NOMEM:
				fprintf(stderr, "Error: Out of memory when trying to publish message.\n");
				break;
			case MOSQ_ERR_NO_CONN:
				fprintf(stderr, "Error: Client not connected when trying to publish.\n");
				break;
			case MOSQ_ERR_PROTOCOL:
				fprintf(stderr, "Error: Protocol error when communicating with broker.\n");
				break;
			case MOSQ_ERR_PAYLOAD_SIZE:
				fprintf(stderr, "Error: Message payload is too large.\n");
				break;
			}
			mosquitto_disconnect(mosq);
		}
	}
	else {
		if (result) {
			fprintf(stderr, "%s\n", mosquitto_connack_string(result));
		}
	}
}

void my_disconnect_callback(struct mosquitto *mosq, void *obj, int rc)
{
	printf("Disconnected!\n");
	connected = false;
}

void my_publish_callback(struct mosquitto *mosq, void *obj, int mid)
{
	printf("Published!\n");
	if (disconnect_sent == false) {
		mosquitto_disconnect(mosq);
		disconnect_sent = true;
	}
}
//__________________________________push_____________________________________________

bool SDK_Start(char* cafile, char* Certfile, char* Keyfile)
{
	/* Required before calling other mosquitto functions */
	mosquitto_lib_init();

	/* Create a new client instance.
	* id = NULL -> ask the broker to generate a client id for us
	* clean session = true -> the broker should remove old sessions when we connect
	* obj = NULL -> we aren't passing any of our private data for callbacks
	*/
	mosq = mosquitto_new(NULL, true, NULL);
	if (mosq == NULL) {
		fprintf(stderr, "Error: Out of memory.\n");
		return false;
	}

	/* Configure callbacks. This should be done before connecting ideally. */
	mosquitto_connect_callback_set(mosq, on_connect);
	mosquitto_subscribe_callback_set(mosq, on_subscribe);
	mosquitto_message_callback_set(mosq, on_message);

	//mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_disconnect_callback_set(mosq, my_disconnect_callback);
	mosquitto_publish_callback_set(mosq, my_publish_callback);

	m_cafile = cafile;
	m_Certfile = Certfile;
	m_Keyfile = Keyfile;
	return true;
}
int Init_connect(char* MQTT_HOSTNAME, int MQTT_PORT, char*MQTT_USERNAME, char*MQTT_PASSWORD, char*MQTT_TOPIC_V2)
{
	mosquitto_username_pw_set(mosq, MQTT_USERNAME, MQTT_PASSWORD);

	//mosquitto_tls_set(mosq, m_cafile, NULL, m_Certfile, m_Keyfile, NULL);

	int ret = mosquitto_connect(mosq, MQTT_HOSTNAME, MQTT_PORT, 60);
	if (ret) {
		printf("Cant connect to mosquitto server 1\n");
		return 0;
	}

	/* Run the network loop in a blocking call. The only thing we do in this
	* example is to print incoming messages, so a blocking call here is fine.
	*
	* This call will continue forever, carrying automatic reconnections if
	* necessary, until the user calls mosquitto_disconnect().
	*/
	mosquitto_loop_forever(mosq, -1, 1);

	//threadObj = std::thread(thread_function);
	//threadObj.detach();

	return 1;
}

int Close_connect()
{
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	std::cout << "close connetc" << std::endl;
	return 1;
}


bool SDK_Recv(char* &data, char* sor)
{
	std::string temp;
	
}

int main()
{
	//测试订阅
	bool retstat = SDK_Start(nullptr,nullptr,nullptr);
	int ret1 = Init_connect((char *)"127.0.0.1", 1883, (char*)"lsq", (char*)"lsq", nullptr);
	if (!ret1)
	{
		Close_connect();
	}
    return 0;
}

