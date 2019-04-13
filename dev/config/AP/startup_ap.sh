#!/bin/bash

clear

echo "== Setting Up Brypt Network 802.11 Access Point"

echo "== Storing Old Configuration Files"

echo "== Loading New Configuration Files"

echo "== Enabling hostapd and dynsmasq"


echo "== Setup Completed!"
echo "== Your System Needs to be Rebooted. This will happen automatically in 10 seconds."

cd /home/pi
touch /home/pi/interfaces
echo "source-directory /etc/network/interfaces.d" >> /home/pi/interfaces
echo "auto wlan0" >> /home/pi/interfaces
echo "iface wlan0 inet static" >> /home/pi/interfaces
echo "address 192.168.30.1" >> /home/pi/interfaces
echo "netmask 255.255.255.0\n" >> /home/pi/interfaces
echo "auto eth0" >> /home/pi/interfaces
echo "iface eth0 inet dhcp" >> /home/pi/interfaces

sudo rm /etc/network/interfaces
sudo mv /home/pi/interfaces /etc/network

sleep 10s
shutdown -h now -r
