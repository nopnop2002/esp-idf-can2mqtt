#define	PUBLISH		100
#define	SUBSCRIBE	200

typedef struct {
	int16_t topic_type;
	int16_t topic_len;
	char topic[64];
	int16_t data_len;
	char data[64];
} MQTT_t;

typedef struct {
	uint16_t frame;
	uint32_t canid;
	char * topic;
	int16_t topic_len;
} TOPIC_t;

