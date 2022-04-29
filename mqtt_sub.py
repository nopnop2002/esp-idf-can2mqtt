#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# python -m pip install -U paho-mqtt

import paho.mqtt.client as mqtt

# MQTT Broker
host = 'broker.emqx.io'
host = '192.168.10.40'
# MQTT Port
port = 1883
# MQTT Subscribe Topic
topic = '/can/#'

def on_connect(client, userdata, flags, respons_code):
	print('connect {0} status {1}'.format(host, respons_code))
	client.subscribe(topic)

def on_message(client, userdata, msg):
	print('topic={}'.format(msg.topic))
	print('payload={}'.format(msg.payload))

if __name__=='__main__':
	client = mqtt.Client(protocol=mqtt.MQTTv311)
	client.on_connect = on_connect
	client.on_message = on_message
	client.connect(host, port=port, keepalive=60)
	client.loop_forever()
