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

static const char *TAG = "PUB";

static EventGroupHandle_t s_mqtt_event_group;

extern const uint8_t root_cert_pem_start[] asm("_binary_root_cert_pem_start");
extern const uint8_t root_cert_pem_end[] asm("_binary_root_cert_pem_end");

#define MQTT_CONNECTED_BIT BIT0

extern QueueHandle_t xQueue_mqtt_tx;
extern QueueHandle_t xQueue_twai_tx;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
#else
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
#endif
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	esp_mqtt_event_handle_t event = event_data;
#endif
	switch (event->event_id) {
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
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
			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			break;
		default:
			ESP_LOGI(TAG, "Other event id:%d", event->event_id);
			break;
	}
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
	return ESP_OK;
#endif
}

esp_err_t query_mdns_host(const char * host_name, char *ip);
void convert_mdns_host(char * from, char * to);

void mqtt_pub_task(void *pvParameters)
{
#if CONFIG_ENABLE_SECURE_MQTT
	ESP_LOGI(TAG, "Start Subscribe Broker:%s", CONFIG_MQTTS_BROKER);
#else
	ESP_LOGI(TAG, "Start Subscribe Broker:%s", CONFIG_MQTT_BROKER);
#endif

	/* Create Eventgroup */
	s_mqtt_event_group = xEventGroupCreate();
	configASSERT( s_mqtt_event_group );
	xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);

	// Set client id from mac
	uint8_t mac[8];
	ESP_ERROR_CHECK(esp_base_mac_addr_get(mac));
	for(int i=0;i<8;i++) {
		ESP_LOGD(TAG, "mac[%d]=%x", i, mac[i]);
	}
	char client_id[64];
	sprintf(client_id, "pub-%02x%02x%02x%02x%02x%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	ESP_LOGI(TAG, "client_id=[%s]", client_id);

	// Resolve mDNS host name
	char ip[128];
	char uri[138];
#if CONFIG_ENABLE_SECURE_MQTT
	ESP_LOGI(TAG, "CONFIG_MQTTS_BROKER=[%s]", CONFIG_MQTTS_BROKER);
	convert_mdns_host(CONFIG_MQTTS_BROKER, ip);
	ESP_LOGI(TAG, "ip=[%s]", ip);
	sprintf(uri, "mqtts://%s", ip);
#else
	ESP_LOGI(TAG, "CONFIG_MQTT_BROKER=[%s]", CONFIG_MQTT_BROKER);
	convert_mdns_host(CONFIG_MQTT_BROKER, ip);
	ESP_LOGI(TAG, "ip=[%s]", ip);
	sprintf(uri, "mqtt://%s", ip);
#endif
	ESP_LOGI(TAG, "uri=[%s]", uri);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	esp_mqtt_client_config_t mqtt_cfg = {
		.broker.address.uri = uri,
#if CONFIG_ENABLE_SECURE_MQTT
		.broker.verification.certificate = (const char *)root_cert_pem_start,
		.broker.address.port = 8883,
#else
		.broker.address.port = 1883,
#endif
#if CONFIG_BROKER_AUTHENTICATION
		.credentials.username = CONFIG_AUTHENTICATION_USERNAME,
		.credentials.authentication.password = CONFIG_AUTHENTICATION_PASSWORD,
#endif
		.credentials.client_id = client_id
	};

#else
	esp_mqtt_client_config_t mqtt_cfg = {
		.uri = uri,
#if CONFIG_ENABLE_SECURE_MQTT
		.cert_pem = (const char *)root_cert_pem_start,
		.port = 8883,
#else
		.port = 1883,
#endif
		.event_handle = mqtt_event_handler,
#if CONFIG_BROKER_AUTHENTICATION
		.username = CONFIG_AUTHENTICATION_USERNAME,
		.password = CONFIG_AUTHENTICATION_PASSWORD,
#endif
		.client_id = client_id
	};
#endif // ESP_IDF_VERSION

	esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
#endif

	esp_mqtt_client_start(mqtt_client);
	xEventGroupWaitBits(s_mqtt_event_group, MQTT_CONNECTED_BIT, false, true, portMAX_DELAY);
	ESP_LOGI(TAG, "Connect to MQTT Server");

	MQTT_t mqttBuf;
	while (1) {
		xQueueReceive(xQueue_mqtt_tx, &mqttBuf, portMAX_DELAY);
		if (mqttBuf.topic_type == PUBLISH) {
			//ESP_LOGI(TAG, "TOPIC=%.*s\r", mqttBuf.topic_len, mqttBuf.topic);
			ESP_LOGI(TAG, "TOPIC=[%s] LEN=%d", mqttBuf.topic, mqttBuf.data_len);
			//memset(mqttBuf.data, 0, sizeof(mqttBuf.data));
			for(int i=0;i<mqttBuf.data_len;i++) {
				ESP_LOGI(TAG, "DATA=0x%x", mqttBuf.data[i]);
			}
			EventBits_t EventBits = xEventGroupGetBits(s_mqtt_event_group);
			ESP_LOGI(TAG, "EventBits=0x%"PRIx32, EventBits);
			if (EventBits & MQTT_CONNECTED_BIT) {
				esp_mqtt_client_publish(mqtt_client, mqttBuf.topic, mqttBuf.data, mqttBuf.data_len, 1, 0);
			} else {
				ESP_LOGE(TAG, "mqtt broker not connect");
			}
		}
	} // end while

	// Never reach here
	ESP_LOGI(TAG, "Task Delete");
	esp_mqtt_client_stop(mqtt_client);
	vTaskDelete(NULL);
}
