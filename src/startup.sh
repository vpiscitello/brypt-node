#!/bin/bash
# Get serial number
SERIAL=`echo $1 | sudo -S cat /proc/cpuinfo | grep Serial | cut -d ' ' -f 2`
# Get IP address
IP=`hostname -I | cut -d ' ' -f 1`
