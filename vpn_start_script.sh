#!/bin/bash
#echo "------- START VPN"
CONFIG_FILE=$1
grep -qxF 'user nobody' "$CONFIG_FILE" || echo 'user nobody' >> "$CONFIG_FILE"
grep -qxF 'group nogroup' "$CONFIG_FILE" || echo 'group nogroup' >> "$CONFIG_FILE"
openvpn --config "$CONFIG_FILE"