#!/usr/bin/env python3

#import threading
import time
import paho.mqtt.client as mqtt
import argparse

# TODO payload in hex -> transform in int

# The callback for when the gateway receives a CONNACK response from the broker
def on_connect_callback(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

# The callback for when the gateway receives a message from the broker
def on_message_callback(client, userdata, message):
    print("message topic=",message.topic)
    print("payload=" ,str(message.payload.decode("utf-8")))

def main():
    # Describe arguments for -help command
    parser = argparse.ArgumentParser(description="MQTT Client")
    parser.add_argument("subject", metavar="SUBJECTNAME", type=str, help="complete name of the subject to subscribe to")
    parser.add_argument("host", metavar="HOST", type=str, help="hostname or IP address of the broker")

    # Retrieve arguments
    args = parser.parse_args()

    # Hostname or IP address of the remote broker
    host = args.host     
    subject = args.subject       
    
    # Initialise client
    client = mqtt.Client()

    # Set callback function
    client.on_connect = on_connect_callback
    client.on_message = on_message_callback

    # Connect to the broker
    client.connect(host, port=1883, keepalive=60, bind_address="")

     # Subscribe to logs of subscribe/unsubscribe actions
    client.subscribe(subject)

    # Start the loop, to process the callbacks
    client.loop_forever()

if __name__ == '__main__':
    main()
