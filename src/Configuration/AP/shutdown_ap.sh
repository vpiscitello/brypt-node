#!/bin/bash

clear

echo "== Shutting Down Brypt Network 802.11 Access Point"

echo "== Storing Old Configuration Files"

echo "== Loading New Configuration Files"

echo "== Disabling hostapd and dynsmasq"

echo "== Tear Down Completed!"
echo "== Your System Needs to be Rebooted. This will happen automatically in 10 seconds."

sleep 10s
shutdown -h now -r
