#!/usr/bin/env python3

import threading
import time
import paho.mqtt.client as mqtt
import subprocess

topicdict = {
    0x0: "temperature"
    0x1: "swagdensity"
    0x2: "whatAmIDoingWithMyLife"
}

# The callback for when the client receives a CONNACK response from the server.
def on_connect_callback(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

def handle_cmd():
    while True:
        cmd = input("Type any command...\n")
        print(cmd)
        # Send command

def sensors_interface(mqttc):
    # Start serialdump tool
    p = subprocess.Popen("../../tools/sky/serialdump-linux -b115200 /dev/ttyUSB0 ", input = subprocess.PIPE, output = subprocess.PIPE)
    while True:
        stdoutdata, stderrdata = p.communicate()
        if stdoutdata != None:
            # Modify condition + do something
            topic = 
            msg_content = 
            mqttc.publish(topic, payload=msg_content, qos=0, retain=False)
    p.terminate()

def main():
    # Connect to the broker
    host = "lol.com"            # hostname or IP address of the remote broker 
    
    client = mqtt.Client()
    client.on_connect = on_connect_callback

    client.connect(host, port=1883, keepalive=60, bind_address="")

    threading.Thread(target = handle_cmd).start()
    threading.Thread(target=sensors_interface, args=(client)).start()

if __name__ == '__main__':
    main()