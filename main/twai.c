/* TWAI Network Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/twai.h" // Update from V4.2

#include "mqtt.h"

static const char *TAG = "TWAI";

extern QueueHandle_t xQueue_mqtt_tx;
extern QueueHandle_t xQueue_twai_tx;

extern TOPIC_t *publish;
extern int16_t npublish;

void dump_table(TOPIC_t *topics, int16_t ntopic);

void twai_task(void *pvParameters)
{
	ESP_LOGI(TAG,"task start");
	dump_table(publish, npublish);

	twai_message_t rx_msg;
	twai_message_t tx_msg;
	MQTT_t mqttBuf;
	mqttBuf.topic_type = PUBLISH;
	while (1) {
		esp_err_t ret = twai_receive(&rx_msg, pdMS_TO_TICKS(100));
		if (ret == ESP_OK) {
			ESP_LOGD(TAG,"twai_receive identifier=0x%x flags=0x%x data_length_code=%d",
				rx_msg.identifier, rx_msg.flags, rx_msg.data_length_code);

			int ext = rx_msg.flags & 0x01;
			int rtr = rx_msg.flags & 0x02;
			ESP_LOGD(TAG, "ext=%x rtr=%x", ext, rtr);

#if CONFIG_ENABLE_PRINT
			if (ext == 0) {
				printf("Standard ID: 0x%03x     ", rx_msg.identifier);
			} else {
				printf("Extended ID: 0x%08x", rx_msg.identifier);
			}
			printf(" DLC: %d  Data: ", rx_msg.data_length_code);

			if (rtr == 0) {
				for (int i = 0; i < rx_msg.data_length_code; i++) {
					printf("0x%02x ", rx_msg.data[i]);
				}
			} else {
				printf("REMOTE REQUEST FRAME");

			}
			printf("\n");
#endif

			for(int index=0;index<npublish;index++) {
				if (publish[index].frame != ext) continue;
				if (publish[index].canid == rx_msg.identifier) {
					ESP_LOGI(TAG, "publish[%d] frame=%d canid=0x%x topic=[%s] topic_len=%d",
					index, publish[index].frame, publish[index].canid, publish[index].topic, publish[index].topic_len);
					mqttBuf.topic_len = publish[index].topic_len;
					for(int i=0;i<mqttBuf.topic_len;i++) {
						mqttBuf.topic[i] = publish[index].topic[i];
						mqttBuf.topic[i+1] = 0;
					}
					if (rtr == 0) {
						mqttBuf.data_len = rx_msg.data_length_code;
					} else {
						mqttBuf.data_len = 0;
					}
					for(int i=0;i<mqttBuf.data_len;i++) {
						mqttBuf.data[i] = rx_msg.data[i];
					}
					if (xQueueSend(xQueue_mqtt_tx, &mqttBuf, portMAX_DELAY) != pdPASS) {
						ESP_LOGE(TAG, "xQueueSend Fail");
					}
				}
			}

		} else if (ret == ESP_ERR_TIMEOUT) {
			if (xQueueReceive(xQueue_twai_tx, &tx_msg, 0) == pdTRUE) {
				ESP_LOGI(TAG, "tx_msg.identifier=[0x%x] tx_msg.extd=%d", tx_msg.identifier, tx_msg.extd);
				twai_status_info_t status_info;
				twai_get_status_info(&status_info);
				ESP_LOGD(TAG, "status_info.state=%d",status_info.state);
				if (status_info.state != TWAI_STATE_RUNNING) {
					ESP_LOGE(TAG, "TWAI driver not running %d", status_info.state);
					continue;
				}
				ESP_LOGD(TAG, "status_info.msgs_to_tx=%d",status_info.msgs_to_tx);
				ESP_LOGD(TAG, "status_info.msgs_to_rx=%d",status_info.msgs_to_rx);
				//esp_err_t ret = twai_transmit(&tx_msg, pdMS_TO_TICKS(10));
				esp_err_t ret = twai_transmit(&tx_msg, 0);
				if (ret == ESP_OK) {
					ESP_LOGI(TAG, "twai_transmit success");
				} else {
					ESP_LOGE(TAG, "twai_transmit Fail %s", esp_err_to_name(ret));
				}
			}

		} else {
			ESP_LOGE(TAG, "twai_receive Fail %s", esp_err_to_name(ret));
		}
	}

	// never reach
	vTaskDelete(NULL);
}

