#!/usr/bin/env python3

# This file represents the MQTT-Rime Gateway
# Authors: 
# BOSCH Sami 		- 26821500
# SIMON Benjamin 	- 37151500
# SLUYTERS Arthur	- 13151500

import threading
import time
import paho.mqtt.client as mqtt
import subprocess
import argparse

# ("subject name", dictionary of sensors/isActivated, list of receivers)
topicdict = {
    0: ("temperature", {}, set()),
    1: ("swagdensity", {}, set()),
    2: ("whatAmIDoingWithMyLife", {}, set())
}

topicdict_reversed = {}

# TODO Remove sensors after a certain amount of time without SUBSCRIBE or PUBLISH message received from them



# The callback for when the gateway receives a CONNACK response from the broker
def on_connect_callback(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

# The callback for when the gateway receives a message from the broker
def on_message_callback(client, userdata, message):
    msg = str(message.payload.decode("utf-8")).split()
    print(str(message.payload.decode("utf-8")))
    # A client subscribes
    if message.topic == "$SYS/broker/log/M/subscribe":
        sub_id = msg[1]
        topic_name = msg[3]
        print("subscribe message received:", sub_id, topic_name)
        if topic_name in topicdict_reversed:
            # Add the subscriber to the list of clients
            topicdict[topicdict_reversed[topic_name]][2].add(sub_id)
            print(len(topicdict[topicdict_reversed[topic_name]][2]))
    # A client unsubscribes
    elif message.topic == "$SYS/broker/log/M/unsubscribe":
        sub_id = msg[1]
        topic_name = msg[3]
        print("unsubscribe message received:", sub_id, topic_name)
        if topic_name in topicdict_reversed:
            # Remove the client from the list of clients
            topicdict[topicdict_reversed[topic_name]][2].remove(sub_id)
            print(len(topicdict[topicdict_reversed[topic_name]][2]))



# Function that handles commands written by a user on stdin
def handle_cmd(communication_process):
    while True:
        cmd = input("Type any command...\n").strip()
        print(cmd)
        # Send command to the root node (cmd val dst)
        if cmd.upper() == "SEND PERIODICALLY":
            communication_process.stdin.write("0 1 -1\n")
        elif cmd.upper() == "SEND ON CHANGE":
            communication_process.stdin.write("0 0 -1\n")
        else:
            print("Unknown command. Try typing SEND PERIODICALLY or SEND ON CHANGE")



# Function that handles communication between the sensor network, this gateway and the broker
def sensors_interface(mqttc, communication_process):
    # Start serialdump tool, read each line
    for line in communication_process.stdout:
        data = line.split()
        if data[0] == "PUBLISH":
            sensor_id = int(data[1])
            subject_id = int(data[2])
            msg_content = data[3]
            # Add the sensor to the list of sensors for this subject
            if sensor_id not in topicdict[subject_id][1]:
                topicdict[subject_id][1][sensor_id] = False
            # If no subscriber, send a control message to stop sending data, else publish data
            if len(topicdict[subject_id][2]) == 0:
                # Set the sensor as deactivated + send a control message to deactivate it
                topicdict[subject_id][1][sensor_id] = False
                communication_process.stdin.write("1 0 {:d}\n".format(sensor_id))
            elif topicdict[subject_id][1][sensor_id] == False:
                # Set the sensor as active + re-send a control message to activate it to make sure that the sensor is indeed activated 
                topicdict[subject_id][1][sensor_id] = True
                communication_process.stdin.write("1 1 {:d}\n".format(sensor_id))
            else:
                mqttc.publish(topicdict[subject_id][0], payload=msg_content, qos=0, retain=False)
        elif data[0] == "ADVERTISE":
            sensor_id = int(data[1])
            subject_id = int(data[2])
            # Add the sensor to the list of sensors for this subject
            if sensor_id not in topicdict[subject_id][1]:
                topicdict[subject_id][1][sensor_id] = False
                # If no subscriber, send a control message to stop sending data
            if len(topicdict[subject_id][2]) == 0:
                if (topicdict[subject_id][1][sensor_id] == True):
                    # Set the sensor as deactivated + send a control message to deactivate it
                    topicdict[subject_id][1][sensor_id] = False
                    communication_process.stdin.write("1 0 {:d}\n".format(sensor_id))
            elif topicdict[subject_id][1][sensor_id] == False:
                # Send a control message to activate the sensor 
                communication_process.stdin.write("1 1 {:d}\n".format(sensor_id))
        elif data[0] == "DELETE":
            sensor_id = int(data[1])
            # Remove the sensor from each subject
            print("Removing sensor {:d}".format(sensor_id))
            for key, val in topicdict.items():
                val[1].pop(sensor_id, None)
                
    communication_process.terminate()



def main():
    # Initialize reversed dictionary
    for key, val in topicdict.items():
        topicdict_reversed[val[0]] = key
    
    # Get hostname and serial device from arguments
    # Describe arguments for -help command
    parser = argparse.ArgumentParser(description="MQTT-Rime gateway")
    parser.add_argument("serialdevice", metavar="SERIALDEVICE", type=str, help="path of the serial device (e.g. '/dev/pts/1')")
    parser.add_argument("host", metavar="HOST", type=str, help="hostname or IP address of the broker")

    # Retrieve arguments
    args = parser.parse_args()
    
    # Serial device (to connect with the root node)
    serialdevice = args.serialdevice
    
    # Hostname or IP address of the remote broker
    host = args.host            
    
    # Initialise client
    client = mqtt.Client("Gateway")
    
    # Set callback function
    client.on_connect = on_connect_callback
    client.on_message = on_message_callback

    # Connect to the broker
    client.connect(host, port=1883, keepalive=60, bind_address="")
    print("ok2")
    # Subscribe to logs of subscribe/unsubscribe actions
    client.subscribe("$SYS/broker/log/M/subscribe")
    client.subscribe("$SYS/broker/log/M/unsubscribe")

    # Launch the serialdump tool for communication with the root node
    p = subprocess.Popen(["../../tools/sky/serialdump-linux", "-b115200", serialdevice], stdout=subprocess.PIPE, stdin=subprocess.PIPE, bufsize=1, universal_newlines=True)

    # Launch threads to handle commands and messages from the root node
    threading.Thread(target=handle_cmd, args=(p,)).start()
    threading.Thread(target=sensors_interface, args=(client, p)).start()

    # Start the loop, to process the callbacks
    client.loop_forever()



if __name__ == '__main__':
    main()
