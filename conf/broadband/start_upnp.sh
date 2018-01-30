#!/bin/sh
##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2016 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
#
. /etc/device.properties
. /etc/include.properties
#. /etc/config.properties
#. /etc/env_setup.sh
if [ -f /SetEnv.sh ]; then
    . /SetEnv.sh
fi
. $RDK_PATH/utils.sh
if [ -f $RDK_PATH/commonUtils.sh ]; then
   . $RDK_PATH/commonUtils.sh
fi
WAREHOUSE_ENV="$RAMDISK_PATH/warehouse_mode_active"
export LD_LIBRARY_PATH=/usr/local/lib:/usr/lib:/lib:$LD_LIBRARY_PATH
if [ "$FS_DBUS_SIGNAL_HANDLER" = "false" ]; then
    export FSARGS="no-sighandler"
    export DFBARGS="no-sighandler"
fi
wifi_interface=""
# initial cleanup
killall xdiscovery
if [ "$GATEWAY_DEVICE" = "yes" ];then
     killall xcal-device
fi
# inputs arguments
XDISC_LOG_FILE="/rdklogs/logs/xdevice.log"
if [ "$1" != "" ]; then
     XDISC_LOG_FILE="$1"
fi
#XDEVICE_LOG="$LOG_PATH/xdevice.log"
echo "before zcip"
ip addr add 169.254.1.1/16 brd + dev brlan0 label brlan0:0
#zcip -f -q -v brlan0 /lib/rdk/zcip.script
zcip -f -q -v brlan0 /tmp/zcip.script
echo "after zcip"
XCAL_DEF_INTERFACE=brlan0:0
if [ "$DEVICE_TYPE" != "mediaclient" ]; then
    if [ -f /opt/upnp_interface ] ; then
         export XCAL_DEF_INTERFACE=`cat /opt/upnp_interface`
    else
         export XCAL_DEF_INTERFACE=brlan0:0
    fi
    if [ ! "$XCAL_DEF_INTERFACE" ] ; then
         export XCAL_DEF_INTERFACE=brlan0:0
    fi
else
    XCAL_DEF_INTERFACE=`getMoCAInterface`
fi
if [ -f /tmp/wifi-on ]; then
    XCAL_DEF_INTERFACE=`getWiFiInterface`
fi
if [ -f /opt/xupnp ];then
      echo "/opt/xupnp is a file, cleaning for creating the folder..!"
      rm -rf /opt/xupnp
fi
# setup ENV
if [ -d /opt/xupnp ]; then
     rm -rf /opt/xupnp
fi
mkdir -p /etc/xupnp
stbIp=""
mocaIpWait()
{
  while [ -z $stbIp ]
  do
   stbIp=`ifconfig $XCAL_DEF_INTERFACE | grep inet | tr -s ' ' | cut -d ' ' -f3 | sed -e 's/addr://g'`
   sleep 1
   echo "MoCA ipaddress is not assigned, waiting to get the STB Ipaddress"
  done
  echo "Got MOCA Ip Address: $stbIp"
}
iarmBusInitializationWait()
{
  loop=1
  while [ $loop -eq 1 ]
  do
     if [ -f $RAMDISK_PATH/.IarmBusMngrFlag ]; then
          loop=0
          echo "IARM is up, ready to start start-upnp module"
          #rm -rf $RAMDISK_PATH/.IarmBusMngrFlag
     else
         sleep 1
         echo "Waiting for IARM manager binaries..(start-upnp)!"
     fi
done
}
estbIpWait()
{
   loop=1
   while [ $loop -eq 1 ]
   do
       estbIp=`getIPAddress`
       if [ "X$estbIp" == "X" ]; then
           sleep 10
       else
           if [ "$IPV6_ENABLED" = "true" ]; then
                if [ "Y$estbIp" != "Y$DEFAULT_IP" ] && [ -f $WAREHOUSE_ENV ]; then
                   loop=0
                elif [ ! -f /tmp/estb_ipv4 ] && [ ! -f /tmp/estb_ipv6 ]; then
                   sleep 10
                elif [ "Y$estbIp" == "Y$DEFAULT_IP" ] && [ -f /tmp/estb_ipv4 ]; then
                   #echo "waiting for IP ..."
                   sleep 10
                else
                   loop=0
                fi
           else
                if [ "Y$estbIp" == "Y$DEFAULT_IP" ]; then
                    #echo "waiting for IP ..."
                    sleep 10
                else
                    loop=0
                fi
          fi
      fi
done
}
# configuration setup
#ip addr add 169.254.0.0/16 brd + dev brlan0 label brlan0:0
#zcip -f -q -v brlan0 /etc/zcip.script

#xcalDeviceConfig="/etc/xdevice.conf"
xcalDeviceConfig="/tmp/xdevice.conf"
if [ "$BUILD_TYPE" != "prod" ] && [ -f /etc/xdevice.conf ]; then
     xcalDeviceConfig="/etc/xdevice.conf"
fi
#xcalDiscoveryConfig="/etc/xdiscovery.conf"
xcalDiscoveryConfig="/tmp/xdiscovery.conf"
if [ "$BUILD_TYPE" != "prod" ] && [ -f /etc/xdiscovery.conf ]; then
     xcalDiscoveryConfig="/etc/xdiscovery.conf"
fi
if [ "$DEVICE_TYPE" = "hybrid" ]; then
     if [ -f "/etc/xupnp/xdevice_hybrid.conf" ]; then
          xcalDeviceConfig="/etc/xupnp/xdevice_hybrid.conf"
     fi
     if [ "$BUILD_TYPE" != "prod" ] && [ -f /etc/xdevice_hybrid.conf ]; then
          xcalDeviceConfig="/etc/xdevice_hybrid.conf"
     fi
fi
echo "Moca interface =  $XCAL_DEF_INTERFACE"
if [ "$GATEWAY_DEVICE" = "yes" ] || [ "$DEVICE_TYPE" = "hybrid" ]; then
     # regular and hybrid
     if [ ! -s /etc/xupnp/BasicDevice.xml ] || [ ! -f /etc/xupnp/BasicDevice.xml ] ; then
#       cp /etc/xupnp/BasicDevice.xml /opt/xupnp/BasicDevice.xml
       chmod +x /etc/xupnp/BasicDevice.xml
       loop=1
       count=0
       RETRY_COUNT=4
       RETRY_DELAY=3
       while [ $loop -eq 1 ]
       do
          if  [ ! -s /etc/xupnp/BasicDevice.xml ] ; then
               count=$((count + 1))
               if [ $count -ge $RETRY_COUNT ] ; then
                    echo "start_upnp: $RETRY_COUNT tries failed. Giving up..."
                    loop=0
               else
                    echo "start_upnp: Sleeping for $RETRY_DELAY more seconds ..."
                    sleep $RETRY_DELAY
               fi
          else
               echo "start_upnp: File size non-zero, good to go ..."
               loop=0
          fi
      done
   fi
   if [ -f /etc/xupnp/DiscoverFriendlies.xml ]; then
        cp /etc/xupnp/DiscoverFriendlies.xml  /opt/xupnp/DiscoverFriendlies.xml
   fi
   if [ -f /etc/xupnp/RemoteUIServerDevice.xml ]; then
         cp /etc/xupnp/RemoteUIServerDevice.xml /opt/xupnp/RemoteUIServerDevice.xml
         chmod +x /opt/xupnp/RemoteUIServerDevice.xml
   fi
   if [ -f /etc/xupnp/RemoteUIServer.xml ]; then
         cp /etc/xupnp/RemoteUIServer.xml /opt/xupnp/RemoteUIServer.xml
   fi
   if [ -f /etc/MediaServer.xml ]; then
         cp /etc/MediaServer.xml /opt/xupnp/MediaServer.xml
         chmod +x /opt/xupnp/MediaServer.xml
   fi
   if [ -f /etc/ContentDirectory.xml ]; then
         cp /etc/ContentDirectory.xml /opt/xupnp/ContentDirectory.xml
   fi
   if [ -f /etc/ConnectionManager.xml ]; then
         cp /etc/ConnectionManager.xml /opt/xupnp/ConnectionManager.xml
   fi
else
   echo "Mediaclient Execution..!"
fi
mocaIpWait
INTERFACE_TEST=`ifconfig $XCAL_DEF_INTERFACE`
if [ "X$INTERFACE_TEST" == "X" ] ; then
        echo "XCAL-UPnP - Selected UPnP interface $XCAL_DEF_INTERFACE does not exist ... cannot start Services"
        exit 1
else
        INTERFACE_IP=`ifconfig $XCAL_DEF_INTERFACE | grep 'inet addr:' | cut -d: -f2 | awk '{print $1}'`
        if [ "X$INTERFACE_IP" == "X" ] ; then
                echo "XCAL-UPnP - Selected UPnP interface $XCAL_DEF_INTERFACE does not have a valid ip address ... cannot start Services"
                exit 1
        fi
fi
echo "Starting Xcal UPNP client xcal-discovery-list"
# IARM BUS initialization wait
#iarmBusInitializationWait
/usr/bin/xdiscovery $xcalDiscoveryConfig $XDISC_LOG_FILE $XCAL_DEF_INTERFACE  &
/usr/bin/xcal-device  &
if [ "$GATEWAY_DEVICE" = "yes" ] || [ "$DEVICE_TYPE" = "hybrid" ] ;then
     estbIpWait
     /usr/bin/xcal-device $xcalDeviceConfig $XDEVICE_LOG &
     # Enable traffic on MOCA interface for ports used by xupnp on gateway devices
#     if [ -f /lib/rdk/updateFireWall.sh ]; then
#         /lib/rdk/updateFireWall.sh xcal-device
#         /lib/rdk/updateFireWall.sh xdiscovery
#     fi
fi
sleep 30
loop=0
flag=0
while [ $loop -eq 0 ]
do
   output1=`pidof xdiscovery`
   if [ ! "$output1" ]; then
        /usr/bin/xdiscovery $xcalDiscoveryConfig $XDISC_LOG_FILE $XCAL_DEF_INTERFACE &
        flag=1
   fi
   if [ "$GATEWAY_DEVICE" = "yes" ] || [ "$DEVICE_TYPE" = "hybrid" ];then
           output1=`pidof xcal-device`
           if [ ! "$output1" ]; then
 #                /usr/bin/xcal-device $xcalDeviceConfig $XDEVICE_LOG &
                /usr/bin/xcal-device &
                 flag=1
           fi
   fi
   if [ "$HDD_ENABLED" = "true" ] && [ $flag -eq 1 ]; then
        sleep 1
        export crashTS=`date +%Y-%m-%d-%H-%M-%S`
        if [ "$POTOMAC_SVR" != "" ] && [ "$BUILD_TYPE" != "dev" ]; then
           nice -n 19 sh $RDK_PATH/uploadDumps.sh $crashTS 1 $POTOMAC_SVR &
        else
           nice -n 19 sh $RDK_PATH/uploadDumps.sh $crashTS 1&
        fi
        flag=0
   fi
   sleep 30
done

