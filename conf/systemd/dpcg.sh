#!/bin/sh
cn="icebergwedge_dp"
fname=icebergwedge_t
kname=icebergwedge_y
dptmp=/tmp
dpcerts=/etc/xupnp/dp

#if [ -f $dptmp/$fname ]; then 
#     if [ -f $dptmp/$kname ]; then  
#       exit 0;
#     fi
#fi
if [ -f $dptmp/$fname.csr ]; then rm -rf $dptmp/$fname.csr; fi
if [ -f $dptmp/$fname ]; then rm -rf $dptmp/$fname; fi
if [ -f $dptmp/$kname ]; then rm -rf $dptmp/$kname; fi
if [ -f $dptmp/icebergwedge-ca-chain.pem ]; then rm -rf $dptmp/icebergwedge-ca-chain.pem; fi
if [ -f $dptmp/index.txt ]; then rm -rf $dptmp/index.txt; fi

touch $dptmp/index.txt
#compute certificate begin date and end date.
d0=`date +"%s"`
s=`expr 24 \* 3600`
d=20190101184654Z
echo "start date:" $d
s2=`expr $s \* 365`
d2=`expr $d0 \+ $s2`

e=`date -d @$(( $d2)) +"%Y%m%d%H%M%SZ"`
echo "end date:" $e

cp $dpcerts/icebergwedge-ca-chain.pem $dptmp/icebergwedge

#generate keys
openssl genrsa -out $dptmp/$kname 2048

#generate certificate signing request
openssl req -new -sha256 -key $dptmp/$kname -out $dptmp/$fname.csr -subj "/C=US/ST=Pennsylvania/O=Comcast Corporation/OU=CPT/CN=Comcast RDK Iceberg Wedge Intermediate CA/emailAddress=rdksiteam-security@rdklistmgr.ccp.xcal.tv"

configparamgen jx gxwxdsvutjug $dptmp/gxwxdsvut

#generate a certificate for the device using the csr
openssl ca -batch -in $dptmp/$fname.csr -cert $dpcerts/icebergwedge.pem -keyfile $dptmp/gxwxdsvut -out $dptmp/$fname -startdate $d -enddate $e -md sha256 -noemailDN  -config $dptmp/openssl.cnf -outdir $dptmp -noemailDN -extensions server_cert -create_serial -notext
sleep 5s
sync
sync

if [ -f $dptmp/gxwxdsvut ]; then rm -rf $dptmp/gxwxdsvut; fi

rm -rf $dptmp/*.pem
chmod -R 600 $dptmp/$fname
chmod -R 600 $dptmp/$kname

sleep 5
