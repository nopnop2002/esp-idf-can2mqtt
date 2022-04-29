#!/bin/bash
mosquitto_sub -h broker.emqx.io -p 1883 -t '/can/#' -F %X -d
