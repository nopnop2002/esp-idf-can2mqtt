/*	TWAI Network Example

	This example code is in the Public Domain (or CC0 licensed, at your option.)

	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/twai.h" // Update from V4.2

#include "mqtt.h"

static const char *TAG = "TWAI_V5";

extern QueueHandle_t xQueue_mqtt_tx;
extern QueueHandle_t xQueue_twai_tx;

extern TOPIC_t *publish;
extern int16_t npublish;

void dump_table(TOPIC_t *topics, int16_t ntopic);

static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

#if CONFIG_CAN_BITRATE_25
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_25KBITS();
#define BITRATE "Bitrate is 25 Kbit/s"
#elif CONFIG_CAN_BITRATE_50
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_50KBITS();
#define BITRATE "Bitrate is 50 Kbit/s"
#elif CONFIG_CAN_BITRATE_100
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_100KBITS();
#define BITRATE "Bitrate is 100 Kbit/s"
#elif CONFIG_CAN_BITRATE_125
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
#define BITRATE "Bitrate is 125 Kbit/s"
#elif CONFIG_CAN_BITRATE_250
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
#define BITRATE "Bitrate is 250 Kbit/s"
#elif CONFIG_CAN_BITRATE_500
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
#define BITRATE "Bitrate is 500 Kbit/s"
#elif CONFIG_CAN_BITRATE_800
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_800KBITS();
#define BITRATE "Bitrate is 800 Kbit/s"
#elif CONFIG_CAN_BITRATE_1000
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS();
#define BITRATE "Bitrate is 1 Mbit/s"
#endif

static const twai_general_config_t g_config =
	TWAI_GENERAL_CONFIG_DEFAULT(CONFIG_CTX_GPIO, CONFIG_CRX_GPIO, TWAI_MODE_NORMAL);

// Format and print the twai message
void twai_print_frame(twai_message_t frame) {
	int ext = frame.extd;
	int rtr = frame.rtr;

	if (ext == 0) {
		printf("Standard ID: 0x%03"PRIx32"%*s", frame.identifier, 5, "");
	} else {
		printf("Extended ID: 0x%08"PRIx32, frame.identifier);
	}
	printf("  DLC: %d Data: ", frame.data_length_code);

	if (rtr == 0) {
		for (int i = 0; i < frame.data_length_code; i++) {
			printf("0x%02x ", frame.data[i]);
		}
	} else {
		printf("REMOTE REQUEST FRAME");
	}
	printf("\n");
}

void twai_task(void *pvParameters)
{
	ESP_LOGI(TAG, "Start");
	ESP_LOGI(TAG, "TWAI_BITRATE=%d",CONFIG_TWAI_BITRATE);
	ESP_LOGI(TAG, "CTX_GPIO=%d",CONFIG_CTX_GPIO);
	ESP_LOGI(TAG, "CRX_GPIO=%d",CONFIG_CRX_GPIO);

	ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
	ESP_LOGI(TAG, "Driver installed");
	ESP_ERROR_CHECK(twai_start());
	ESP_LOGI(TAG, "Driver started");

	dump_table(publish, npublish);

	FRAME_t sendFrame;
	MQTT_t mqttBuf;
	mqttBuf.topic_type = PUBLISH;
	bool running = true;
	while (running) {
		twai_message_t rx_msg;
		esp_err_t ret = twai_receive(&rx_msg, pdMS_TO_TICKS(10));
		if (ret == ESP_OK) {
			ESP_LOGD(TAG,"twai_receive identifier=0x%"PRIx32" data_length_code=%d",
				rx_msg.identifier, rx_msg.data_length_code);
			int extd = rx_msg.extd;
			int rtr = rx_msg.rtr;
			ESP_LOGD(TAG, "extd=%x rtr=%x", extd, rtr);

#if CONFIG_ENABLE_PRINT
			twai_print_frame(rx_msg);
#endif

			for(int index=0;index<npublish;index++) {
				if (publish[index].frame != extd) continue;
				if (publish[index].canid == rx_msg.identifier) {
					ESP_LOGI(TAG, "publish[%d] frame=%d canid=0x%"PRIx32" topic=[%s] topic_len=%d",
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
					memset(mqttBuf.data, 0, sizeof(mqttBuf.data));
					for(int i=0;i<mqttBuf.data_len;i++) {
						mqttBuf.data[i] = rx_msg.data[i];
						ESP_LOGI(TAG, "mqttBuf.data[i]=0x%x", mqttBuf.data[i]);
					}
					if (xQueueSend(xQueue_mqtt_tx, &mqttBuf, portMAX_DELAY) != pdPASS) {
						ESP_LOGE(TAG, "xQueueSend Fail");
						running = false;
					}
				}
			} // end for

		} else if (ret == ESP_ERR_TIMEOUT) {
			if (xQueueReceive(xQueue_twai_tx, &sendFrame, 0) == pdPASS) {
				ESP_LOGI(TAG, "sendFrame.canid=[0x%"PRIx32"] sendFrame.extd=%d", sendFrame.canid, sendFrame.extd);
				twai_status_info_t status_info;
				twai_get_status_info(&status_info);
				ESP_LOGD(TAG, "status_info.state=%d",status_info.state);
				if (status_info.state != TWAI_STATE_RUNNING) {
					ESP_LOGE(TAG, "TWAI driver not running %d", status_info.state);
					running = false;
					continue;
				}
				ESP_LOGD(TAG, "status_info.msgs_to_tx=%"PRIu32, status_info.msgs_to_tx);
				ESP_LOGD(TAG, "status_info.msgs_to_rx=%"PRIu32, status_info.msgs_to_rx);

				twai_message_t tx_msg;
				tx_msg.extd = sendFrame.extd;
				tx_msg.rtr = 0;
				tx_msg.ss = 1;
				tx_msg.self = 0;
				tx_msg.dlc_non_comp = 0;
				tx_msg.identifier = sendFrame.canid;
				tx_msg.data_length_code = sendFrame.data_len;
				for (int i=0;i<tx_msg.data_length_code;i++) {
					tx_msg.data[i] = sendFrame.data[i];
				}

				esp_err_t ret = twai_transmit(&tx_msg, 0);
				if (ret == ESP_OK) {
					ESP_LOGI(TAG, "twai_transmit success");
				} else {
					ESP_LOGE(TAG, "twai_transmit Fail %s", esp_err_to_name(ret));
					running = false;
				}
			}

		} else {
			ESP_LOGE(TAG, "twai_receive Fail %s", esp_err_to_name(ret));
			running = false;
		}
	} // end while

	ESP_ERROR_CHECK(twai_stop());
	ESP_ERROR_CHECK(twai_driver_uninstall());
	vTaskDelete(NULL);
}
