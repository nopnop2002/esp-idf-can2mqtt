#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# python3 -m pip install -U paho-mqtt
# python3 -m pip install -U argparse

import argparse
import random
import paho.mqtt.client as mqtt

def on_connect(client, userdata, flags, respons_code, properties):
	print('connect {0} status {1}'.format(args.host, respons_code))
	client.subscribe(args.topic)

def on_message(client, userdata, msg):
	print('topic={}'.format(msg.topic))
	print('payload={}'.format(msg.payload))

if __name__=='__main__':
	parser = argparse.ArgumentParser()
	parser.add_argument('--host', help='mqtt broker', default='broker.emqx.io')
	parser.add_argument('--port', type=int, help='mqtt port', default=1883)
	parser.add_argument('--topic', help='mqtt topic', default='/can/#')
	args = parser.parse_args() 
	print("args.host={}".format(args.host))
	print("args.port={}".format(args.port))
	print("args.topic={}".format(args.topic))

	client_id = f'python-mqtt-{random.randint(0, 1000)}'
	client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id)
	client.on_connect = on_connect
	client.on_message = on_message
	client.connect(args.host, port=args.port, keepalive=60)
	client.loop_forever()
