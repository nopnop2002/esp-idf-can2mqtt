/*
	This code is in the Public Domain (or CC0 licensed, at your option.)

	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "driver/twai.h"

#include "mqtt.h"

static const char *TAG = "SUB";

extern const uint8_t root_cert_pem_start[] asm("_binary_root_cert_pem_start");
extern const uint8_t root_cert_pem_end[] asm("_binary_root_cert_pem_end");

static EventGroupHandle_t s_mqtt_event_group;
#define MQTT_CONNECTED_BIT BIT0

extern QueueHandle_t xQueue_mqtt_tx;
extern QueueHandle_t xQueue_twai_tx;

extern TOPIC_t *subscribe;
extern int16_t nsubscribe;

void dump_table(TOPIC_t *topics, int16_t ntopic);

static QueueHandle_t xQueueSubscribe;


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	esp_mqtt_event_handle_t event = event_data;
	switch (event->event_id) {
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
			//esp_mqtt_client_subscribe(mqtt_client, CONFIG_SUB_TOPIC, 0);
			break;
		case MQTT_EVENT_DISCONNECTED:
			ESP_LOGW(TAG, "MQTT_EVENT_DISCONNECTED");
			xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
			break;
		case MQTT_EVENT_SUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_UNSUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_PUBLISHED:
			ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_DATA:
			ESP_LOGI(TAG, "MQTT_EVENT_DATA");
			//ESP_LOGI(TAG, "TOPIC=%.*s\r", event->topic_len, event->topic);
			//ESP_LOGI(TAG, "DATA=%.*s\r", event->data_len, event->data);
			MQTT_t mqttBuf;
			mqttBuf.topic_type = SUBSCRIBE;
			mqttBuf.topic_len = event->topic_len;
			for(int i=0;i<event->topic_len;i++) {
				mqttBuf.topic[i] = event->topic[i];
				mqttBuf.topic[i+1] = 0;
			}
			mqttBuf.data_len = event->data_len;
			for(int i=0;i<event->data_len;i++) {
				mqttBuf.data[i] = event->data[i];
			}
			xQueueSend(xQueueSubscribe, &mqttBuf, 0);
			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			break;
		default:
			ESP_LOGI(TAG, "Other event id:%d", event->event_id);
			break;
	}
	return;
}

esp_err_t query_mdns_host(const char * host_name, char *ip);
void convert_mdns_host(char * from, char * to);

void mqtt_sub_task(void *pvParameters)
{
	ESP_LOGI(TAG, "Start Subscribe Broker:%s", CONFIG_MQTT_BROKER);
	dump_table(subscribe, nsubscribe);

	/* Create Eventgroup */
	s_mqtt_event_group = xEventGroupCreate();
	configASSERT( s_mqtt_event_group );
	xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);

	/* Create Queue */
	xQueueSubscribe = xQueueCreate( 10, sizeof(MQTT_t) );
	configASSERT( xQueueSubscribe );

	// Set client id from mac
	uint8_t mac[8];
	ESP_ERROR_CHECK(esp_base_mac_addr_get(mac));
	for(int i=0;i<8;i++) {
		ESP_LOGD(TAG, "mac[%d]=%x", i, mac[i]);
	}
	char client_id[64];
	sprintf(client_id, "sub-%02x%02x%02x%02x%02x%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	ESP_LOGI(TAG, "client_id=[%s]", client_id);

	// Resolve mDNS host name
	char ip[128];
	char uri[128];
	ESP_LOGI(TAG, "CONFIG_MQTT_BROKER=[%s]", CONFIG_MQTT_BROKER);
	convert_mdns_host(CONFIG_MQTT_BROKER, ip);
	ESP_LOGI(TAG, "ip=[%s]", ip);
#if CONFIG_MQTT_TRANSPORT_OVER_TCP
	ESP_LOGI(TAG, "MQTT_TRANSPORT_OVER_TCP");
	sprintf(uri, "mqtt://%.60s:%d", ip, CONFIG_MQTT_PORT_TCP);
#elif CONFIG_MQTT_TRANSPORT_OVER_SSL
	ESP_LOGI(TAG, "MQTT_TRANSPORT_OVER_SSL");
	sprintf(uri, "mqtts://%.60s:%d", ip, CONFIG_MQTT_PORT_SSL);
#elif CONFIG_MQTT_TRANSPORT_OVER_WS
	ESP_LOGI(TAG, "MQTT_TRANSPORT_OVER_WS");
	sprintf(uri, "ws://%.60s:%d/mqtt", ip, CONFIG_MQTT_PORT_WS);
#elif CONFIG_MQTT_TRANSPORT_OVER_WSS
	ESP_LOGI(TAG, "MQTT_TRANSPORT_OVER_WSS");
	sprintf(uri, "wss://%.60s:%d/mqtt", ip, CONFIG_MQTT_PORT_WSS);
#endif
	ESP_LOGI(TAG, "uri=[%s]", uri);

	// Initialize MQTT configuration structure
	esp_mqtt_client_config_t mqtt_cfg = {
		.broker.address.uri = uri,
#if CONFIG_MQTT_TRANSPORT_OVER_TCP
#elif CONFIG_MQTT_TRANSPORT_OVER_SSL
		.broker.verification.certificate = (const char *)root_cert_pem_start,
#elif CONFIG_MQTT_TRANSPORT_OVER_WS
#elif CONFIG_MQTT_TRANSPORT_OVER_WSS
		.broker.verification.certificate = (const char *)root_cert_pem_start,
#endif
#if CONFIG_BROKER_AUTHENTICATION
		.credentials.username = CONFIG_AUTHENTICATION_USERNAME,
		.credentials.authentication.password = CONFIG_AUTHENTICATION_PASSWORD,
#endif
		.credentials.client_id = client_id
	};

	esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
	esp_mqtt_client_start(mqtt_client);
	xEventGroupWaitBits(s_mqtt_event_group, MQTT_CONNECTED_BIT, false, true, portMAX_DELAY);
	ESP_LOGI(TAG, "Connect to MQTT Server");
	
	for(int index=0;index<nsubscribe;index++) {
		ESP_LOGI(TAG, "subscribe[%d] topic=[%s]", index, subscribe[index].topic);
		esp_mqtt_client_subscribe(mqtt_client, subscribe[index].topic, 0);
	}

	twai_message_t tx_msg;
	MQTT_t mqttBuf;
	while (1) {
		xQueueReceive(xQueueSubscribe, &mqttBuf, portMAX_DELAY);
		ESP_LOGI(TAG, "type=%d", mqttBuf.topic_type);

		if (mqttBuf.topic_type != SUBSCRIBE) continue;
		//ESP_LOGI(TAG, "TOPIC=%.*s\r", mqttBuf.topic_len, mqttBuf.topic);
		ESP_LOGI(TAG, "TOPIC=[%s]", mqttBuf.topic);
		for(int i=0;i<mqttBuf.data_len;i++) {
			ESP_LOGI(TAG, "DATA=0x%x", mqttBuf.data[i]);
		}

		for(int index=0;index<nsubscribe;index++) {
			if (strcmp(subscribe[index].topic, mqttBuf.topic) != 0) continue;
			ESP_LOGI(TAG, "subscribe[index].frame=%d", subscribe[index].frame);
			tx_msg.extd = subscribe[index].frame;
			tx_msg.ss = 1;
			tx_msg.self = 0;
			tx_msg.dlc_non_comp = 0;
			tx_msg.identifier = subscribe[index].canid;
			tx_msg.data_length_code = mqttBuf.data_len;
			if (mqttBuf.data_len > 8) {
				ESP_LOGW(TAG, "Data length is reduced to 8 bytes");
				tx_msg.data_length_code = 8;
			}
			for (int i=0;i<tx_msg.data_length_code;i++) {
				tx_msg.data[i] = mqttBuf.data[i];
			}
			if (xQueueSend(xQueue_twai_tx, &tx_msg, portMAX_DELAY) != pdPASS) {
				ESP_LOGE(pcTaskGetName(0), "xQueueSend Fail");
			}
		}

	} // end while

	// Never reach here
	ESP_LOGI(TAG, "Task Delete");
	esp_mqtt_client_stop(mqtt_client);
	vTaskDelete(NULL);
}
