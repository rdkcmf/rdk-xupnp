/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef XDISCOVERY_PRIVATE_H_
#define XDISCOVERY_PRIVATE_H_

#define G_LOG_DOMAIN "XUPNP"
#define XUPNP_RESCAN_INTERVAL 10000000
#define XDISC_SERVICE "urn:schemas-upnp-org:service:DiscoverFriendlies:1"

struct ProxyMapping {
    GUPnPDeviceProxy *proxy;
    GUPnPServiceProxyAction *action;
    GSource *timeout_src;
};

typedef struct _gwyDeviceData {
    GString* serial_num;
    gboolean isgateway;
    gboolean requirestrm;
    GString* gwyip;
    GString* gwyipv6;
    GString* gatewaystbip;
    GString* ipv6prefix;
    GString* devicename;
    GString* bcastmacaddress;
    GString* hostmacaddress;
    GString* recvdevtype;
    GString* devicetype;
    GString* buildversion;
    GString* dsgtimezone;
    gint rawoffset;
    gint dstoffset;
    gint dstsavings;
    gboolean usesdaylighttime;
    GString* baseurl;
    GString* basetrmurl;
    GString* playbackurl;
    GString* fogtsburl;
    GString* videobaseurl;
    GString* dataGatewayIPaddress;
    GString* dnsconfig;
    GString* etchosts;
    GString* systemids;
    GString* receiverid;
    gboolean devFoundFlag;
    gboolean isRouteSet;
    gboolean isOwnGateway;
} GwyDeviceData;

GList* xdevlist= NULL;
//const char* host_ip = "eth0";
const guint host_port = 50759;

GMutex *mutex;
GMutex *devMutex;
GCond *cond;
GMainContext *main_context;
GUPnPContext *context;
GUPnPControlPoint* cp;
GString* outputfilename;
GString* outputcontents;
GString* ipMode;
FILE *logoutfile;
GString *ownSerialNo;
char ipaddress[INET6_ADDRSTRLEN];

typedef struct
{
    gchar *discIf,*gwIf;
    gchar *gwSetupFile,*logFile,*outputJsonFile;
    gboolean enableGwSetup;
    gint GwPriority;
} ConfSettings;

ConfSettings *disConf;


#ifdef USE_XUPNP_TZ_UPDATE
struct _Timezone
{
    char* upnpTZ;
    char* tz;
    char* tzInDst;
}m_timezoneinfo[] = 
{
    {"US/Hawaii", "HST11", "HST11HDT,M3.2.0,M11.1.0"},
    {"US/Alaska", "AKST",   "AKST09AKDT,M3.2.0,M11.1.0"},
    {"US/Pacific", "PST08", "PST08PDT,M3.2.0,M11.1.0"},
    {"US/Mountain", "MST07","MST07MDT,M3.2.0,M11.1.0"},
    {"US/Central", "CST06","CST06CDT,M3.2.0,M11.1.0"},
    {"US/Eastern", "EST05","EST05EDT,M3.2.0,M11.1.0"},
};
#endif

void* verify_devices();
gboolean init_gwydata(GwyDeviceData* gwydata);
gboolean sendDiscoveryResult(const char* outfilename);
void delOldItemsFromList(gboolean bDeleteAll);

void xupnp_logger (const gchar *log_domain, GLogLevelFlags log_level,
                   const gchar *message, gpointer user_data);

gboolean verifyDeviceAttributeChanges(GUPnPServiceProxy *, GwyDeviceData *);
static void on_last_change (GUPnPServiceProxy *sproxy, const char  *variable_name, GValue  *value, gpointer user_data);
gboolean readconffile(const char* configfile);
gboolean getserialnum(GString* ownSerialNo);
int getipaddress(const char* ifname, char* ipAddressBuffer, gboolean ipv6Enabled);
char *replace_string(char *src_string, char *sub_string, char *replace_string);
gboolean replace_hn_with_local(GwyDeviceData* gwydata);
gboolean replace_local_device_ip(GwyDeviceData* gwydata);
gboolean checkvalidip( char* ipAddress);
gboolean checkvalidhostname( char* hostname);
void broadcastIPModeChange(void);

#endif /* XDISCOVERY_PRIVATE_H_ */
