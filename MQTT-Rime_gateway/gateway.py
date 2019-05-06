#!/usr/bin/env python3

import threading
import time
import paho.mqtt.client as mqtt
import subprocess

# ("subject name", list of sensors, list of receivers)
topicdict = {
    0: ("temperature", [], []),
    1: ("swagdensity", [], []),
    2: ("whatAmIDoingWithMyLife", [], [])
}

# TODO Send a SENSOR_CONTROL message to a sensor when a subscriber subscribes to reactivate it
# TODO Remove sensors after a certain amount of time without SUBSCRIBE or PUBLISH message received from them

# The callback for when the gateway receives a CONNACK response from the broker
def on_connect_callback(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

# The callnback for when the gateway receives a message from the broker
def on_message_callback(client, userdata, message):
    # TODO Add/remove subscribers
    print("message topic=",message.topic)
    if message.topic == "$SYS/broker/log/M/subscribe":
        print("subscribe message received " ,str(message.payload.decode("utf-8")))
    elif message.topic == "$SYS/broker/log/M/unsubscribe":
        print("unsubscribe message received " ,str(message.payload.decode("utf-8")))

def handle_cmd(communication_process):
    while True:
        cmd = input("Type any command...\n").strip()
        print(cmd)
        # Send command to the root node (cmd val dst)
        if cmd.upper() == "SEND PERIODICALLY":
            communication_process.stdin.write("0 1 -1")
        elif cmd.upper() == "SEND ON CHANGE":
            communication_process.stdin.write("0 0 -1")
        else:
            print("Unknown command. Try typing SEND PERIODICALLY or SEND ON CHANGE")

def sensors_interface(mqttc, communication_process):
    # Start serialdump tool, read each line
    for line in communication_process.stdout:
        print(line, end='')
        data = line.split()
        if data[0] == "PUBLISH":
            sensor_id = int(data[1])
            subject_id = int(data[2])
            msg_content = data[3]
            # Add the sensor to the list of sensors for this subject
            if sensor_id not in topicdict[subject_id][1]:
                topicdict[subject_id][1].append(sensor_id)
            # If no subscriber, send a control message to stop sending data, else publish data
            if len(topicdict[subject_id][2]) == 0:
                communication_process.stdin.write("1 0 {:d}".format(sensor_id))
            else:
                mqttc.publish(topicdict[subject_id][0], payload=msg_content, qos=0, retain=False)
        elif data[0] == "ADVERTISE":
            sensor_id = int(data[1])
            subject_id = int(data[2])
            # Add the sensor to the list of sensors for this subject
            if sensor_id not in topicdict[subject_id][1]:
                topicdict[subject_id][1].append(sensor_id)
                # If no subscriber, send a control message to stop sending data
                if len(topicdict[subject_id][2]) == 0:
                    communication_process.stdin.write("1 0 {:d}".format(sensor_id))
    communication_process.terminate()

def main():
    # Hostname or IP address of the remote broker
    host = "lol.com"             
    
    # Initialise client
    client = mqtt.Client()

    # Set callback function
    client.on_connect = on_connect_callback
    client.on_message=on_message_callback

    # Connect to the broker
    client.connect(host, port=1883, keepalive=60, bind_address="")

    # Subscribe to logs of subscribe/unsubscribe actions
    client.subscribe("$SYS/broker/log/M/subscribe")
    client.subscribe("$SYS/broker/log/M/unsubscribe")

    # Launch the serialdump tool for communication with the root node
    p = subprocess.Popen(["../../tools/sky/serialdump-linux", "-b115200", "/dev/ttyUSB0"], stdout=subprocess.PIPE, stdin=subprocess.PIPE, bufsize=1, universal_newlines=True)

    # Launch threads to handle commands and messages from the root node
    threading.Thread(target=handle_cmd, args=(p)).start()
    threading.Thread(target=sensors_interface, args=(client, p)).start()

    # Start the loop, to process the callbacks
    client.loop_forever()

if __name__ == '__main__':
    main()