#!/bin/bash

nmea_checksum() {
    local input="$1"
    local cksum=0
    for (( i=0; i<${#input}; i++ )); do
        cksum=$(( cksum ^ $(printf "%d" "'${input:$i:1}") ))
    done
    printf "%02X" $cksum
}

echo "Download EPO FILE"
EPO=${1-MTK14.EPO}

file=${2-$EPO}

if [ -e $file ] ; then
	mv $file $file.last
fi

#gps_only
wget -O${file} http://wepodownload.mediatek.com/EPO_GPS_3_7.DAT -t 1 -T 10 || cp $file.last $file

#3 days gps+glonass
# wget -O${file} http://wepodownload.mediatek.com/EPO_GR_3_1.DAT -t 1 -T 10 || cp $file.last $file

echo "Parsing and Extracting Information From EPO"
/home/x_user/my_camera_project/epo_parser /home/x_user/my_camera_project/MTK14.EPO 

echo "Reseting The GPS"
/home/x_user/my_camera_project/gps_config

echo "Injection Time In The GPS"
# Get current time
NOW=$(date +"%Y,%m,%d,%H,%M,%S")
CMD="PMTK740,$NOW"

# Calculate NMEA checksum (without $ and *)
CKSUM=$(nmea_checksum "$CMD")

# Full command string
NMEA="\$$CMD*$CKSUM\r\n"

# Send to GPS serial
echo -ne "$NMEA" > /dev/ttymxc1

sudo /home/x_user/my_camera_project/epo_upload_demo

echo "Read GPS Data"
/home/x_user/my_camera_project/gps_parser

# sudo minicom -D /dev/ttymxc1 -b 115200