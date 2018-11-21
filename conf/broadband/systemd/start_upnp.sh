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
x=""
while [ "started" != "$x" ]
do
   x=`sysevent get lan-status`
   echo "sysevent status = $x"
   sleep 2
done
status=""
while [ "X$status" == "X" ]
do
        status=`psmcli get dmsb.l2net.HomeNetworkIsolation`
        if [ $status == "0" ];
        then
                echo "Moca Isolation disabled creating new interface brlan0:0"
		busybox zcip brlan0 /lib/rdk/zcip.script
		MOCA_INTERFACE="brlan0:0"
        else
                echo "Moca Isolation Enabled, using brlan10 interface"
		MOCA_INTERFACE="brlan10"
        fi
        echo "Success"
done

. /etc/device.properties
. /etc/include.properties
if [ -f /SetEnv.sh ]; then
    . /SetEnv.sh
fi
. $RDK_PATH/utils.sh
if [ -f $RDK_PATH/commonUtils.sh ]; then
   . $RDK_PATH/commonUtils.sh
fi
WAREHOUSE_ENV="$RAMDISK_PATH/warehouse_mode_active"
LOG_PATH="/rdklogs/logs"
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
XDISC_LOG_FILE=""
if [ "$1" != "" ]; then
     XDISC_LOG_FILE="$1"
fi

if [ "$DEVICE_TYPE" != "mediaclient" ]; then
    if [ -f /opt/upnp_interface ] ; then
         export XCAL_DEF_INTERFACE=`cat /opt/upnp_interface`
    else
         export XCAL_DEF_INTERFACE=$MOCA_INTERFACE
    fi
    if [ ! "$XCAL_DEF_INTERFACE" ] ; then
         export XCAL_DEF_INTERFACE=brlan0:0
    fi
else
    XCAL_DEF_INTERFACE=$MOCA_INTERFACE
fi
if [ -f /tmp/wifi-on ]; then
    XCAL_DEF_INTERFACE=`getWiFiInterface`
fi

mkdir -p /nvram/xupnp
stbIp=""
mocaIpWait()
{
  while [ -z $stbIp ]
  do
   stbIp=`ifconfig $XCAL_DEF_INTERFACE | grep inet | tr -s ' ' | cut -d ' ' -f3 | sed -e 's/addr://g'`
   sleep 2
   echo "MoCA ipaddress is not assigned, waiting to get the STB Ipaddress"
  done
  echo "Got MOCA Ip Address: $stbIp"
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

xcalDeviceConfig="/etc/xupnp/xdevice.conf"
if [ "$BUILD_TYPE" != "prod" ] && [ -f /etc/xdevice.conf ]; then
     xcalDeviceConfig="/etc/xdevice.conf"
fi
xcalDiscoveryConfig="/etc/xdiscovery.conf"
if [ "$BUILD_TYPE" != "prod" ] && [ -f /etc/xdiscovery.conf ]; then
     xcalDiscoveryConfig="/etc/xdiscovery.conf"
fi

echo "Moca interface =  $XCAL_DEF_INTERFACE"
if [ "$GATEWAY_DEVICE" = "yes" ] || [ "$DEVICE_TYPE" = "hybrid" ]; then
     # regular and hybrid
     if [ ! -s /etc/xupnp/BasicDevice.xml ] || [ ! -f /etc/xupnp/BasicDevice.xml ] ; then
       cp /etc/xupnp/BasicDevice.xml /opt/xupnp/BasicDevice.xml
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
   if [ -s /etc/xupnp/BasicDevice.xml ] || [ -f /etc/xupnp/BasicDevice.xml ]; then
	cp /etc/xupnp/BasicDevice.xml /nvram/xupnp/BasicDevice.xml
	chmod +x /etc/xupnp/BasicDevice.xml
   fi
   if [ -f /etc/xupnp/DiscoverFriendlies.xml ]; then
        cp /etc/xupnp/DiscoverFriendlies.xml  /nvram/xupnp/DiscoverFriendlies.xml
   fi
   if [ -f /etc/xupnp/RemoteUIServerDevice.xml ]; then
         cp /etc/xupnp/RemoteUIServerDevice.xml /nvram/xupnp/RemoteUIServerDevice.xml
         chmod +x /nvram/xupnp/RemoteUIServerDevice.xml
   fi
   if [ -f /etc/xupnp/RemoteUIServer.xml ]; then
         cp /etc/xupnp/RemoteUIServer.xml /nvram/xupnp/RemoteUIServer.xml
   fi
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

