#!/usr/bin/env python3

import threading
import time

def handle_cmd():
    while True:
        cmd = input("Type any command...\n")
        print(cmd)
        # Send command

def sensors_interface():
    pass
    # Register sensor + subject, transmit data

def subscribers_interface():
    pass
    # Register subscribers

def main():
    threading.Thread(target = handle_cmd).start()
    threading.Thread(target = sensors_interface).start()
    threading.Thread(target = subscribers_interface).start()

if __name__ == '__main__':
    main()