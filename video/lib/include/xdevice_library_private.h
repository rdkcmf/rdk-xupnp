#ifndef __XDEVICE_LIBRARY_PRIVATE_H__
#define __XDEVICE_LIBRARY_PRIVATE_H__
#include <glib.h>
#define MAX_DEBUG_MESSAGE 50
#if defined(USE_XUPNP_IARM_BUS)
#include "libIARM.h"
#endif
typedef struct
{
  gchar *bcastIf, *streamIf, *trmIf, *gwIf, *cvpIf, *ruiPath, *uriOverride, *hostMacIf;
  gchar *oemFile, *dnsFile, *dsgFile, *diagFile, *hostsFile, *wbFile, *devXmlPath, *devXmlFile, *cvpXmlFile, *logFile, *authServerUrl,*devPropertyFile,*ipv6FileLocation,*ipv6PrefixFile,*deviceNameFile;
  gboolean enableCVP2, useIARM, allowGwy, enableTRM, useGliDiag, disableTuneReady,enableHostMacPblsh,rmfCrshSupp,wareHouseMode;
  gint bcastPort, cvpPort;
} ConfSettings;
ConfSettings *devConf;
GString *url, *trmurl, *playbackurl, *gwyip, *gwyipv6, *dnsconfig, *systemids, *serial_num, *lan_ip, *recv_id,*partner_id,*hostmacaddress,*devicetype,*recvdevtype,*buildversion,*ipv6prefix,*gwystbip,*bcastmacaddress,*devicename,*mocaIface,*wifiIface,*fogtsburl,*dataGatewayIPaddress, *videobaseurl;
GString *trmurlCVP2, *playbackurlCVP2, *gwyipCVP2;
unsigned long channelmap_id, dac_id, plant_id, vodserver_id;
GString *dsgtimezone, *etchosts;
gboolean isgateway, tune_ready, service_ready, requirestrm, usesDaylightTime;
gint dstOffset, rawOffset, dstSavings;
GString *ruiurl, *inDevProfile, *uiFilter;
FILE *logoutfile;

//typedef enum _IARM_Result_t
//{
//  IARM_RESULT_SUCCESS,
//  IARM_RESULT_INVALID_PARAM, /*!< Invalid input parameter */
//  IARM_RESULT_INVALID_STATE, /*!< Invalid state encountered */
//  IARM_RESULT_IPCCORE_FAIL,  /*!< Underlying IPC failure */
//  IARM_RESULT_OOM,           /*!< Memory allocation failure */

//} IARM_Result_t;


typedef void (*xupnpEventCallback)(const char*,const char*);
void xupnpEventCallback_register(xupnpEventCallback callback_proc);

typedef struct _STRING_MAP {
    char *pszKey;
    char *pszValue;
} STRING_MAP;
//typedef int IARM_EventId_t;

#if defined(USE_XUPNP_IARM_BUS)
#define  _IARM_XDEVICE_NAME   "XDEVICE" /*!< Method to Get the Xdevice Info */
static void _fogEventHandler(const char *owner, IARM_EventId_t eventId,
                             void *data, size_t len);
static void _sysEventHandler(const char *owner, IARM_EventId_t eventId,
                             void *data, size_t len);
IARM_Result_t _SysModeChange(void *arg);
static void _routesysEventHandler(const char *owner, IARM_EventId_t eventId,
                                  void *data, size_t len);
BOOL XUPnP_IARM_Init(void);
BOOL getRouteData(void);
void getSystemValues(void);
BOOL getFogStatus(void);
//fog
typedef struct _IARM_Bus_FOG_Param_t
{
	BOOL status; // if true, FOG is active
	int fogVersion;
	char tsbEndpoint[33]; // i.e. http://169.254.228.194:9080/tsb?
} IARM_Bus_Fog_Param_t;
#define IARM_BUS_FOG_NAME "FOG"
#define IARM_BUS_FOG_getCurrentState "getCurrentState"
typedef enum
{
	IARM_BUS_FOG_EVENT_STATUS,
	IARM_BUS_FOG_EVENT_MAX
} FOG_EventId_t;
//netsrvmgr
#define IARM_BUS_NM_SRV_MGR_NAME "NET_SRV_MGR"
#define IARM_BUS_NETSRVMGR_Route_Event "sendCurrentRouteData"
#define IARM_BUS_ROUTE_MGR_API_getCurrentRouteData "getCurrentRouteData"
typedef struct _routeEventData_t {
        char routeIp[46];
        gboolean ipv4;
        char routeIf[10];
} routeEventData_t;
typedef struct _IARM_Bus_RouteSrvMgr_RouteData_Param_t {
    routeEventData_t route;
    BOOL status;
} IARM_Bus_RouteSrvMgr_RouteData_Param_t;
typedef enum _NetworkManager_Route_EventId_t {
        IARM_BUS_NETWORK_MANAGER_EVENT_ROUTE_DATA=10,
        IARM_BUS_NETWORK_MANAGER_EVENT_ROUTE_MAX,
} IARM_Bus_NetworkManager_Route_EventId_t;
#endif
BOOL xdeviceInit(char *devConfFile, char *devLogFile);

GString *getID( const gchar *id );
BOOL updatesystemids(void);
BOOL parsedevicename(void);
BOOL parseipv6prefix(void);
static int isVidiPathEnabled();
BOOL readconffile(const char *configfile);
static GString *get_uri_value();
BOOL getetchosts(void);
BOOL parseserialnum(GString *serial_num);
unsigned long getidfromdiagfile(const gchar *diagparam,
                                const gchar *diagfilecontents);
BOOL parsednsconfig(void);
gchar *getmacaddress(const gchar *ifname);
int getipaddress(const char *ifname, char *ipAddressBuffer,gboolean ipv6Enabled);
BOOL check_empty(char *str);
BOOL check_null(char *str);
void mapTimeZoneToJavaFormat(char *payload);
static char *getStrValueFromMap(char *pszKey, int nPairs, STRING_MAP map[]);
static char * getPartnerName();
static char * getFriendlyName();
static char * getProductName();
static char * getServiceName();
static char * getServiceDescription();
static char * getGatewayName();
#else
#error "! __XDEVICE_LIBRARY_PRIVATE_H__"
#endif
