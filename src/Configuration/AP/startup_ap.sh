#!/bin/bash

# Replace old rc.local, so reboot doesn't occur again
sudo cp /home/pi/brypt-node/dev/config/AP/rc.local.base /etc/rc.local

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

if [ -f /root/start_ap ]; then
				# Copy and configure static IP address
				cp /home/pi/brypt-node/dev/config/AP/dhcpcd.conf.on /etc/dhcpcd.conf
				sudo service dhcpcd restart

				# Copy the dnsmasq configuration
				sudo mv /etc/dnsmasq.conf /etc/dnsmasq.conf.orig
				sudo cp /home/pi/brypt-node/dev/config/AP/dnsmasq.conf.on /etc/dnsmasq.conf

				# Set the SSID name
				RAND=`echo $RANDOM | md5sum | cut -b 1-5`
				sudo echo "ssid=brypt-net-$RAND" >> /home/pi/brypt-node/dev/config/AP/hostapd.conf.on
				sudo cp /home/pi/brypt-node/dev/config/AP/hostapd.conf.on /etc/hostapd/hostapd.conf

				# Tell hostapd where to find the config file
				sudo cp /home/pi/brypt-node/dev/config/AP/default-hostapd.on /etc/default/hostapd

				# Ensure hostapd stays live
				sudo cp /home/pi/brypt-node/dev/config/AP/hostapd.service /etc/systemd/system/hostapd.service

				# Restart the services
				sudo systemctl start hostapd
				sudo systemctl start dnsmasq

				# Add routing and masquerade
				sudo cp /home/pi/brypt-node/dev/config/AP/sysctl.conf.on /etc/sysctl.conf
				sudo iptables -t nat -A  POSTROUTING -o eth0 -j MASQUERADE
				sudo sh -c "iptables-save > /etc/iptables.ipv4.nat"
				sudo cp /home/pi/brypt-node/dev/config/AP/rc.local.on /etc/rc.local

				# Clean up remaining file
				sudo rm /root/start_ap
				sudo reboot
fi
