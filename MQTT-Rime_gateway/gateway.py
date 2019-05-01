#!/usr/bin/env python3

import threading
import time
import paho.mqtt.client as mqtt
import subprocess

topicdict = {
    0: "temperature",
    1: "swagdensity",
    2: "whatAmIDoingWithMyLife"
}

# TODO If no receiver for this data, send a SENSOR_CONTROL message to the sensor(s) sending this data to disable it

# The callback for when the client receives a CONNACK response from the server.
def on_connect_callback(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

def handle_cmd(communication_process):
    while True:
        cmd = input("Type any command...\n").strip()
        print(cmd)
        # Send command
        if cmd.upper() == "SEND PERIODICALLY":
            # Send command to root node (cmd val dst)
            communication_process.stdin.write("0 1 -1")
        elif cmd.upper() == "SEND ON CHANGE":
            # Send command to root node
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
            mqttc.publish(topicdict(subject_id), payload=msg_content, qos=0, retain=False)
    communication_process.terminate()

def main():
    # Connect to the broker
    host = "lol.com"            # hostname or IP address of the remote broker 
    
    client = mqtt.Client()
    client.on_connect = on_connect_callback

    client.connect(host, port=1883, keepalive=60, bind_address="")

    p = subprocess.Popen(["../../tools/sky/serialdump-linux", "-b115200", "/dev/ttyUSB0"], stdout=subprocess.PIPE, stdin=subprocess.PIPE, bufsize=1, universal_newlines=True)

    threading.Thread(target = handle_cmd, args=(p)).start()
    threading.Thread(target=sensors_interface, args=(client, p)).start()

if __name__ == '__main__':
    main()