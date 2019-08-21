#!/bin/sh
. /etc/device.properties
if [ "$DEVICE_TYPE" == "broadband" ]; then
    refactor=`syscfg get Refactor`
else
    refactor=`tr181 -g Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.UPnP.Refactor.Enable 2>&1 `
fi
if [ "$refactor" != "true" ]; then
    echo "refactor RFC not Enabled!"
    exit 0
fi    
refactorStartupCheck=1

if [ -f /etc/xupnp/openssl.cnf ]; then
        cp /etc/xupnp/openssl.cnf /tmp/openssl.cnf
else
        echo "File Not Found /etc/xupnp/openssl.cnf"
        refactorStartupCheck=0
fi

if [ -f /etc/xupnp/dpcg.sh ]; then
        cp /etc/xupnp/dpcg.sh /tmp/dpcg.sh
        chmod 700 /tmp/dpcg.sh
        if [ ! -f /etc/xupnp/dp/icebergwedge-ca-chain.pem ] || [ ! -f /etc/xupnp/dp/icebergwedge.pem ]; then
        	echo "Certs Not Found in /etc/xupnp/dp/"
        	refactorStartupCheck=0
		fi
else
        echo "File Not Found /etc/xupnp/dpcg.sh"
        refactorStartupCheck=0
fi

if [ "$DEVICE_TYPE" == "broadband" ]; then
        if [ "$refactorStartupCheck" == "1" ]; then
                sleep 5s
                count=0
                ntpStarted=`sysevent get wan-status`
                while [ $ntpStarted != "started" ] && [ $count -le 20 ]
                do
                        sleep 5s 
                        count=$((count + 1))
                        ntpStarted=`sysevent get wan-status`
                done
                if [ $count -eq 20 ]; then
                        echo "NTP not synced"
                        syscfg set Refactor false
                        exit 0
                fi
                sleep 30s
        else
                echo "Failed StartupCheck.. Disabling RFC!!"
                syscfg set Refactor false
                exit 0
        fi
else
        if [ "$refactorStartupCheck" == "0" ]; then
            echo "Certs issue disabling refactor RFC"
            tr181 -s -v false Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.UPnP.Refactor.Enable 2>&1
            exit 0
        fi
fi
/tmp/dpcg.sh
