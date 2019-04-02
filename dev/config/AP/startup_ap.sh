#!/bin/bash

clear

echo "== Setting Up Brypt Network 802.11 Access Point"

echo "== Storing Old Configuration Files"

echo "== Loading New Configuration Files"

echo "== Enabling hostapd and dynsmasq"


echo "== Setup Completed!"
echo "== Your System Needs to be Rebooted. This will happen automatically in 10 seconds."

sleep 10s
shutdown -h now -r
