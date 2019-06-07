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
/**
 * @file xdiscovery.c
 * @brief This source file contains functions  that will run as task for maintaining the device list.
 */
#include <libgupnp/gupnp-control-point.h>
#include <string.h>
#include <unistd.h>
#include <libgssdp/gssdp.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

#include "xdiscovery.h"
#include "xdiscovery_private.h"
#ifdef ENABLE_BREAKPAD
#include "breakpadWrapper.h"
#endif
#ifdef BROADBAND
#include "breakpad_wrapper.h"
#else
#ifdef ENABLE_RFC
#include "rfcapi.h"
#endif
#endif
#define DEVICE_PROTECTION_CONTEXT_PORT  50760
//Symbols defined in makefile (via defs.mk)
//#define USE_XUPNP_IARM
//#define GUPNP_0_19
//#define GUPNP_0_14
const char* localHostIP="127.0.0.1";
static int rfc_enabled ;
static GMainLoop *main_loop;
gboolean checkDevAddInProgress=FALSE;
#define WAIT_TIME_SEC 5
guint deviceAddNo=0;
int getSoupStatusFromUrl(char* url);
#if defined(USE_XUPNP_IARM) || defined(USE_XUPNP_IARM_BUS)

typedef struct _iarmDeviceData {
    unsigned long bufferLength;
    char* pBuffer;
} IarmDeviceData;

//static GMainLoop *main_loop;
#define WAIT_TIME_SEC 5
#ifdef USE_XUPNP_IARM
#include "uidev.h"
static void *gCtx = NULL;
static void _GetXUPNPDeviceInfo(void *callCtx, unsigned long methodID, void *arg, unsigned int serial);
#else
#include "libIBus.h"
#include "libIARMCore.h"
#include "sysMgr.h"
IARM_Result_t _GetXUPNPDeviceInfo(void *arg);
#endif

static IARM_Result_t GetXUPNPDeviceInfo(char *pDeviceInfo, unsigned long length)
{
    g_mutex_lock(devMutex);
    g_message("<<<<<<<IARM XUPnP Call Reached API Processor - length: %ld Output Data length: %ld>>>>>>>>", length, outputcontents->len);

    //IarmDeviceData *param = (IarmDeviceData *)arg;
    if (outputcontents->len <= length)
    {
        //Copy the output to buffer and return success
        g_message("Output string is\n %s\nAddress of received memory is %p", outputcontents->str, pDeviceInfo);
        //Append space for null character
        strncpy (pDeviceInfo, outputcontents->str, outputcontents->len+1);
        g_mutex_unlock(devMutex);
        g_message("<<<<<<<IARM XUPnP Call Returned success from API Processor>>>>>>>>");
        return IARM_RESULT_SUCCESS;
    }
    else
    {
        //return IARM Error
        g_mutex_unlock(devMutex);
        g_message("<<<<<<<IARM XUPnP Call Returned failure from API Processor>>>>>>>>");
        return IARM_RESULT_INVALID_STATE;
    }

    //IARM_CallReturn(callCtx, "UIMgr", "GetXUPNPDeviceInfo", ret, serial);
    //g_mutex_unlock(mutex);

}

//notifyXUPnPDataUpdateEvent(UIDEV_EVENT_XUPNP_DATA_UPDATE)
static void notifyXUPnPDataUpdateEvent(int eventId)
{
#ifdef USE_XUPNP_IARM
    g_message("<<<<<<<IARM XUPnP Posted update request>>>>>>>>");
    UIDev_EventData_t eventData;
    eventData.id = eventId;

    //   struct _UIDEV_XUPNP_DATA {
    //         unsigned long deviceInfoLength;
    //      } xupnpData;
    //Add the length to accommodate NULL character
    g_mutex_lock(devMutex);
    eventData.data.xupnpData.deviceInfoLength = outputcontents->len+1;
    g_mutex_unlock(devMutex);
    UIDev_PublishEvent(eventId, &eventData);
#else

    IARM_Bus_SYSMgr_EventData_t eventData;
    g_mutex_lock(devMutex);
    eventData.data.xupnpData.deviceInfoLength = outputcontents->len+1;
    g_mutex_unlock(devMutex);
    g_message("<<<<<<<IARM XUPnP Posted update request with Length %ld>>>>>>>>",eventData.data.xupnpData.deviceInfoLength);
    IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME, (IARM_EventId_t)IARM_BUS_SYSMGR_EVENT_XUPNP_DATA_UPDATE,(void *)&eventData, sizeof(eventData));

#endif
}

#ifdef USE_XUPNP_IARM

/**
 * @brief If there is any other process wants to get XUPnP data, the event will be handled here.
 *
 * @param[in] eventId Event Identifier identifying the Data request event.
 * @param[in] eventData Optional Event data.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
void _evtHandler(int eventId, void *eventData)
#else

/**
 * @brief This is a call back function for handling SYS Manager events. It notifies the XUPnP
 * about The Data update event triggered by IARM.
 *
 * @param[in] owner Owner of the event, contains name of the Manager.
 * @param[in] eventId Event Identifier identifying the Data request event.
 * @param[in] data Event data.
 * @param[in] len Size of the event data.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
void _evtHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
#endif
{
    g_message("<<<<<<<IARM XUPnP Received update request>>>>>>>>");
#ifdef USE_XUPNP_IARM
    UIDev_EventData_t *evt = eventData;
    if (eventId == UIDEV_EVENT_XUPNP_DATA_REQUEST)
    {
        notifyXUPnPDataUpdateEvent(UIDEV_EVENT_XUPNP_DATA_UPDATE);
    }
#else
    if (strcmp(owner, IARM_BUS_SYSMGR_NAME)  == 0) {
        switch (eventId) {
        case IARM_BUS_SYSMGR_EVENT_XUPNP_DATA_REQUEST:
        {
            notifyXUPnPDataUpdateEvent(IARM_BUS_SYSMGR_EVENT_XUPNP_DATA_UPDATE);
        }
        break;
        default:
            break;
        }
    }
#endif
}

/**
 * @brief This function will do the initialization procedure for using the IARM Bus within XUPnP.
 * This will register the event handlers to receive events from IARM Managers and will connect
 * with the IARM Bus.
 *
 * @return TRUE if initialization is successful.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
gboolean XUPnP_IARM_Init(void)
{
    g_message("<<<<< Iniializing IARM XUPnP >>>>>>>>");

#ifdef USE_XUPNP_IARM
    if (gCtx == NULL) {
        UIDev_Init(_IARM_XUPNP_NAME);
        g_message("<<<<<<<IARM XUPnP Init Done>>>>>>>>");
        if (UIDev_GetContext(&gCtx) == IARM_RESULT_SUCCESS) {
            g_message("<<<<<<<IARM XUPnP Got the context>>>>>>>");
            IARM_RegisterCall(gCtx, "UIMgr", "GetXUPNPDeviceInfo", _GetXUPNPDeviceInfo, NULL/*callCtx*/);
            UIDev_RegisterEventHandler(UIDEV_EVENT_XUPNP_DATA_REQUEST, _evtHandler);
            g_message("<<<<<<<IARM XUPnP Registered the events>>>>>>>>");
            return TRUE;
        }
        else {
            return FALSE;
        }
    }
#else
    if(IARM_RESULT_SUCCESS != IARM_Bus_Init(_IARM_XUPNP_NAME))
    {
        g_message("<<<<<<<%s - Failed in IARM Bus IARM_Bus_Init>>>>>>>>",__FUNCTION__);
        return FALSE;
    }
    else if (IARM_RESULT_SUCCESS != IARM_Bus_Connect())
    {
        g_message("<<<<<<<%s - Failed in  IARM_Bus_Connect>>>>>>>>",__FUNCTION__);
        return FALSE;
    }
    else
    {
        IARM_Bus_RegisterEventHandler(IARM_BUS_SYSMGR_NAME,IARM_BUS_SYSMGR_EVENT_XUPNP_DATA_REQUEST,_evtHandler);
        IARM_Bus_RegisterCall(IARM_BUS_XUPNP_API_GetXUPNPDeviceInfo,_GetXUPNPDeviceInfo);
        g_message("<<<<<<<%s - Registered the BUS Events >>>>>>>>",__FUNCTION__);
        return TRUE;
    }
#endif

}

#ifdef USE_XUPNP_IARM
static void _GetXUPNPDeviceInfo(void *callCtx, unsigned long methodID, void *arg, unsigned int serial)
#else
IARM_Result_t _GetXUPNPDeviceInfo(void *arg)
#endif
{
    g_message("<<<<<<<IARM XUPnP API Call received>>>>>>>>");
    g_mutex_lock(mutex);

#ifdef USE_XUPNP_IARM
    IarmDeviceData *param = (IarmDeviceData *)arg;
    g_message("Address of received memory is %p - Call 0", param->pBuffer);
    IARM_Result_t ret = GetXUPNPDeviceInfo(param->pBuffer, param->bufferLength);
    IARM_CallReturn(gCtx, "UIMgr", "GetXUPNPDeviceInfo", ret, serial);
#else
    // Extract the XUPNP Info data
    IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t *param = (IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t *)arg;
#ifdef USE_DBUS
    GetXUPNPDeviceInfo((char *)param + sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t), param->bufLength);
#else
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    // Create a Mem space for the buff
    retCode = IARM_Malloc(IARM_MEMTYPE_PROCESSSHARE,param->bufLength, &(param->pBuffer));
    g_message("Address of received memory is %p - Call 0", param->pBuffer);
    if(IARM_RESULT_SUCCESS == retCode)
    {
        GetXUPNPDeviceInfo(param->pBuffer, param->bufLength);
    }
#endif
#endif

    g_mutex_unlock(mutex);

    return IARM_RESULT_SUCCESS;

}



#endif //#if defined(USE_XUPNP_IARM) || defined(USE_XUPNP_IARM_BUS)

int check_rfc()
{
#ifndef BROADBAND
#ifdef ENABLE_RFC
    RFC_ParamData_t param = {0};

    WDMP_STATUS status = getRFCParameter("XUPNP","Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.UPnP.Refactor.Enable",&param);

    if (status == WDMP_SUCCESS)
    {
        if (!strncmp(param.value, "true", strlen("true")))
        {
            g_message("New Device Refactoring rfc_enabled");
            return 1;
        }
        else
        {
            g_message("Running older xdiscovery");
        }
    }
    else
    {
        g_message("getRFCParameter Failed : %s\n", getRFCErrorString(status));
    }
#else
    g_message("Not built with RFC support.");
#endif
#else
    syscfg_init();
    char temp[24] = {0};
    if (!syscfg_get(NULL, "Refactor", temp, sizeof(temp)) )
    {
        if(temp != NULL)
        {
            if (strcmp(temp, "true") == 0)
            {
                return 1;
            }
        }
    }
#endif
    return 0;
}

/* get uptime in milliseconds */
unsigned long getUptimeMS(void)
{
    FILE *fp = fopen("/proc/uptime", "r");
    double uptime = 0;
    char buf[256] = {0x0};
    if(fp != NULL)
    {
          fgets(buf, sizeof(buf), fp);
          fclose(fp);
          uptime = atof(buf);
    }
    return uptime*1000;
}

/* log rdk milestones */
void logMilestone(const char *msg_code)
{
    FILE *fp = NULL;
    fp = fopen("/opt/logs/rdk_milestones.log", "a+");
    if (fp != NULL)
    {
      fprintf(fp, "%s:%ld\n", msg_code, getUptimeMS());
      fclose(fp);
    }
}
/**
 * @brief This function is used to log the messages of XUPnP applications. Each Log message
 * will be written to a file along with the timestamp formated in ISO8601 format.
 *
 * @param[in] log_domain Defines the log domain. For applications, this is typically left as
 * the default NULL domain. Libraries should define this so that any messages that they log
 * can be differentiated from messages from other libraries/applications.
 * @param[in] log_level Log level used to categorize messages of type warning, error, info, debug etc.
 * @param[in] message The Message to be logged represented in string format.
 * @param[in] user_data Contains the Log file name.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
void xupnp_logger (const gchar *log_domain, GLogLevelFlags log_level,
                   const gchar *message, gpointer user_data)
{

    GTimeVal timeval;
    gchar *date_str;
    g_get_current_time(&timeval);
    if (logoutfile == NULL)
    {
        // Fall back to console output if unable to open file
        g_print ("%s: g_time_val_to_iso8601(&timeval): %s\n", message);
        return;
    }

    date_str = g_time_val_to_iso8601 (&timeval);
    g_fprintf (logoutfile, "%s : %s\n", date_str, message);
    if(date_str)
    {
        g_free (date_str);
    }
    fflush(logoutfile);
}


static gint
g_list_find_sno(GwyDeviceData* gwData, gconstpointer* sno )
{
    if (g_strcmp0(g_strstrip(gwData->serial_num->str),g_strstrip((gchar *)sno)) == 0)
        return 0;
    else
        return 1;
}

static gint
g_list_find_udn(GwyDeviceData* gwData, gconstpointer* udn )
{
    if (g_strcmp0(g_strstrip(gwData->receiverid->str),g_strstrip((gchar *)udn)) == 0)
        return 0;
    else
        return 1;
}

static gint
g_list_compare_sno(GwyDeviceData* gwData1, GwyDeviceData* gwData2, gpointer user_data )
{
    gint result = g_strcmp0(g_strstrip(gwData1->serial_num->str),g_strstrip(gwData2->serial_num->str));
    return result;
}

/**
 * @brief This will Scan through the device list to check whether a particular device entry exists.
 *
 * It will Match the device serial number in the list to find the desired device entry.
 *
 * @param[in] sno Serial number of the device represented in string format.
 *
 * @return Returns TRUE if serial number is present in the device list else returns FALSE.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
//Find a device with given serial number is in our list of gateways
gboolean checkDeviceExists(const char* sno,char* outPlayUrl)
{
    gboolean retval = FALSE;
    if (g_list_length(xdevlist) > 0)
    {
        GList *element = NULL;
        element = g_list_first(xdevlist);
        while(element)
        {
            gint result = g_strcmp0(g_strstrip(((GwyDeviceData *)element->data)->serial_num->str),g_strstrip(sno));
            //Matched the serial number
            if (result==0)
            {
                retval = TRUE;
                if((((GwyDeviceData *)element->data)->isgateway) == TRUE)
                {
                    strcpy(outPlayUrl,g_strstrip(((GwyDeviceData *)element->data)->playbackurl->str));
                }
                break;
            }
            //Past the search string value in sorted list
            else if (result>0)
            {
                retval = FALSE;
                break;
            }
            //There are still elements to search
            element = g_list_next(element);
        }
//        if (element) g_object_unref(element);
    }
    return retval;
}


static void
device_proxy_unavailable_cb (GUPnPControlPoint *cp, GUPnPDeviceProxy *dproxy)
{
    char gwyPlayUrl[160]={'\0'};
    const gchar* sno = gupnp_device_info_get_serial_number (GUPNP_DEVICE_INFO (dproxy));

    g_message ("In unavailable_cb  Device %s went down",sno);

    GUPnPServiceInfo *sproxy = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), XDISC_SERVICE);

    if (gupnp_service_proxy_get_subscribed(sproxy) == TRUE)
    {
        g_message("Removing notifications on %s", sno);
        gupnp_service_proxy_set_subscribed(sproxy, FALSE);
        gupnp_service_proxy_remove_notify(sproxy, "PlaybackUrl", on_last_change, NULL);
        gupnp_service_proxy_remove_notify(sproxy, "SystemIds", on_last_change, NULL);
    }

    if (checkDeviceExists(sno,gwyPlayUrl))
    {
        if(gwyPlayUrl[0] != '\0')
        {
            if (getSoupStatusFromUrl(gwyPlayUrl))
            {
                g_message("Network Multicast issue for device %s",sno);
                g_free(sno);
                g_object_unref(sproxy);
                return;
            }
            else
            {
                g_message("Can't reach device %s",sno);
            }
        }
        else
        {
            g_message("GW playback url is NULL");
        }

        if (delete_gwyitem(sno) == FALSE)
        {
            g_message("%s found, but unable to delete it from list", sno);
        }
        else
        {
            g_message("Deleted %s from list", sno);
            sendDiscoveryResult(disConf->outputJsonFile);
        }
    }
    g_free(sno);
    g_object_unref(sproxy);
}

static void
device_proxy_unavailable_cb_client (GUPnPControlPoint *cp, GUPnPDeviceProxy *dproxy)
{
    const gchar* sno = gupnp_device_info_get_serial_number (GUPNP_DEVICE_INFO (dproxy));

    g_message ("In unavailable_client Device %s went down",sno);
    GUPnPServiceInfo *sproxy = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), XDISC_SERVICE_MEDIA);
    if (gupnp_service_proxy_get_subscribed(sproxy) == TRUE)
    {
	g_message("Removing notifications on %s", sno);
	gupnp_service_proxy_set_subscribed(sproxy, FALSE);
    }
    if (delete_gwyitem(sno) == FALSE)
    {
        g_message("%s found, but unable to delete it from list", sno);
    }
    else
    {
        g_message("Deleted %s from list", sno);
        sendDiscoveryResult(disConf->outputJsonFile);
    }
    g_free(sno);
    g_object_unref(sproxy);
}

static void
device_proxy_unavailable_cb_gw (GUPnPControlPoint *cp, GUPnPDeviceProxy *dproxy)
{
    const gchar* sno = gupnp_device_info_get_serial_number (GUPNP_DEVICE_INFO (dproxy));
    char gwyPlayUrl[160]={0};
    g_message ("In unavailable_gw Device %s went down",sno);
    GUPnPServiceInfo *sproxy = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), XDISC_SERVICE_MEDIA);
    if (gupnp_service_proxy_get_subscribed(sproxy) == TRUE)
    {
	g_message("Removing notifications on %s", sno);
	gupnp_service_proxy_set_subscribed(sproxy, FALSE);
	gupnp_service_proxy_remove_notify(sproxy, "PlaybackUrl", on_last_change, NULL);
    }
    if (checkDeviceExists(sno, gwyPlayUrl))
    {
        if(gwyPlayUrl[0] != '\0')
        {
            if (getSoupStatusFromUrl(gwyPlayUrl))
            {
                g_message("Network Multicast issue for device %s",sno);
                g_free(sno);
                g_object_unref(sproxy);
                return;
            }
            else
            {
                g_message("Can't reach device %s",sno);
            }
        }
        else
        {
            g_message("GW playback url is NULL");
        }
        if (delete_gwyitem(sno) == FALSE)
        {
            g_message("%s found, but unable to delete it from list", sno);
        }
        else
        {
            g_message("Deleted %s from list", sno);
            sendDiscoveryResult(disConf->outputJsonFile);
        }
    }
    g_free(sno);
    g_object_unref(sproxy);
}

static void
device_proxy_available_cb (GUPnPControlPoint *cp, GUPnPDeviceProxy *dproxy)
{
    //g_print("Found a new device\n");
//    if(!checkDevAddInProgress)
//    {
//    ret=g_mutex_trylock(devMutex);
//    if (TRUE == ret)
//    {
    g_message("Found a new device. deviceAddNo = %u ",deviceAddNo);
    deviceAddNo++;
    if ((NULL==cp) || (NULL==dproxy))
    {
//	        g_mutex_unlock(devMutex);
        deviceAddNo--;
        g_message("WARNING - Received a null pointer for gateway device device no %u",deviceAddNo);
        return;
    }
    if(rfc_enabled)
    {
        gchar* upc = gupnp_device_info_get_upc (GUPNP_DEVICE_INFO (dproxy));
        if(upc)
        {
            if(!(strcmp(upc,"10000")))
            {
                g_message("Exiting the device addition as UPC value matched");
                g_free(upc);
                deviceAddNo--;
                return;
            }
	}
    }
    gchar* sno = gupnp_device_info_get_serial_number (GUPNP_DEVICE_INFO (dproxy));
    GList* xdevlistitem = g_list_find_custom(xdevlist,sno,(GCompareFunc)g_list_find_sno);
    if(xdevlistitem!=NULL)
    {
        deviceAddNo--;
        g_message("Existing as SNO is present in list so no update of devices %s device no %u",sno,deviceAddNo);
        g_free(sno);
        return;
    }
    GwyDeviceData *gwydata = g_new(GwyDeviceData,1);
    if(gwydata)
    {
        init_gwydata(gwydata);
    }
    else
    {
        g_critical("Could not allocate memory for Gwydata. Exiting...");
        exit(1);
    }

    gwydata->sproxy = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), XDISC_SERVICE);
    if(!gwydata->sproxy)
    {
       deviceAddNo--;
       g_message("Unable to get the services, sproxy null. returning");
       return;
    }
    if (sno != NULL)
    {
        //g_print("Device serial number is %s\n",sno);
        g_message("Device serial number is %s",sno);
        g_string_assign(gwydata->serial_num, sno);
        const char* udn = gupnp_device_info_get_udn(GUPNP_DEVICE_INFO (dproxy));
        if (udn != NULL)
        {
            if (g_strrstr(udn,"uuid:"))
            {
                gchar* receiverid = g_strndup(udn+5, strlen(udn)-5);
                if(receiverid)
                {
                    g_message("Device receiver id is %s",receiverid);
                    g_string_assign(gwydata->receiverid, receiverid);
                    g_free(receiverid);

                    if(!process_gw_services(gwydata->sproxy, gwydata))
                    {
		        free_gwydata(gwydata);
                        g_free(gwydata);
                        g_free(sno);
                        deviceAddNo--;
    		        g_message("Exting from device_proxy_available_cb since mandatory paramters are not there  device no %u",deviceAddNo);
		        return;
		    }
                }
                else
                    g_message("Device receiver id is NULL");
            }
        }
        else
            g_message("Device UDN is NULL");
    }

    if(g_strrstr(g_strstrip(gwydata->devicetype->str),"XB") != NULL )
    {
        g_message("Discovered a XB device");
    }
    else
    {
    if(g_strrstr(g_strstrip(gwydata->devicetype->str),"XI") == NULL )
    {
       if (gupnp_service_proxy_add_notify (gwydata->sproxy, "PlaybackUrl", G_TYPE_STRING, on_last_change, NULL) == FALSE)
           g_message("Failed to add url notifications for %s", sno);
       if (gupnp_service_proxy_add_notify (gwydata->sproxy, "SystemIds", G_TYPE_STRING, on_last_change, NULL) == FALSE)
           g_message("Failed to add systemid notifications for %s", sno);
       if (gupnp_service_proxy_add_notify (gwydata->sproxy, "DnsConfig", G_TYPE_STRING, on_last_change, NULL) == FALSE)
           g_message("Failed to add DNS notifications for %s", sno);
       if (gupnp_service_proxy_add_notify (gwydata->sproxy, "TimeZone", G_TYPE_STRING, on_last_change, NULL) == FALSE)
           g_message("Failed to add TimeZone notifications for %s", sno);
    }
    else
    {
       if (gupnp_service_proxy_add_notify (gwydata->sproxy, "DataGatewayIPaddress", G_TYPE_STRING, on_last_change, NULL) == FALSE)
           g_message("Failed to add DataGatewayIPaddress notifications for %s", sno);
    }
    gupnp_service_proxy_set_subscribed(gwydata->sproxy, TRUE);

    if (gupnp_service_proxy_get_subscribed(gwydata->sproxy) == FALSE)
    {
        g_message("Failed to register for notifications on %s", sno);
        g_clear_object(&(gwydata->sproxy));
    }
    }

    g_free(sno);
    deviceAddNo--;
    g_message("Exting from device_proxy_available_cb deviceAddNo = %u",deviceAddNo);

//    }
//    else
//    {
//        g_message("Already Existing device addition going on ");
//    }
//	    g_mutex_unlock(devMutex);
//    }
//    else
//    {
//          g_message("Not able to get the device mutex lock");
//    }
}

static void
device_proxy_available_cb_client (GUPnPControlPoint *cp, GUPnPDeviceProxy *dproxy)
{
    g_message("Found a new Client device. deviceAddNo = %u ",deviceAddNo);
    deviceAddNo++;
    if ((NULL==cp) || (NULL==dproxy))
    {
        deviceAddNo--;
        g_message("WARNING - Received a null pointer for client device device no %u",deviceAddNo);
        return;
    }
    gchar* sno = gupnp_device_info_get_serial_number (GUPNP_DEVICE_INFO (dproxy));
    GList* xdevlistitem = g_list_find_custom(xdevlist,sno,(GCompareFunc)g_list_find_sno);
    if(xdevlistitem!=NULL)
    {
        deviceAddNo--;
        g_message("Existing as client SNO is present in list so no update of devices %s device no %u",sno,deviceAddNo);
        g_free(sno);
        return;
    }

    GwyDeviceData *gwydata = g_new(GwyDeviceData,1);
    init_gwydata(gwydata);
    gwydata->sproxy_i = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), XDISC_SERVICE_IDENTITY);
    gwydata->sproxy_m = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), XDISC_SERVICE_MEDIA);
    if (sno != NULL)
    {
        g_message("XI device serial number is %s",sno);
        g_string_assign(gwydata->serial_num, sno);
        const char* udn = gupnp_device_info_get_udn(GUPNP_DEVICE_INFO (dproxy));
        if (udn != NULL)
        {
            if (g_strrstr(udn,"uuid:"))
            {
                gchar* receiverid = g_strndup(udn+5, strlen(udn)-5);
                if(receiverid)
                {
                    g_message("Client device receiver id is %s",receiverid);
                    g_string_assign(gwydata->receiverid, receiverid);
                    g_free(receiverid);
                    if(!process_gw_services_identity(gwydata->sproxy_i, gwydata))
                    {
                        free_gwydata(gwydata);
                        g_free(gwydata);
                        g_free(sno);
                        deviceAddNo--;
                        g_message("Exting from device_proxy_available_cb_client since mandatory paramters are not there  device no %u",deviceAddNo);
                        return;
                    }
                    else
                    {
			g_message("Received XI Identity service");
                    }
		    if(!process_gw_services_media_config(gwydata->sproxy_m, gwydata))
		    {
			free_gwydata(gwydata);
			g_free(gwydata);
			g_free(sno);
			deviceAddNo--;
			g_message("Exting from device_proxy_available_cb_client since mandatory paramters are not there  device no %u",deviceAddNo);
			return;
		    }
                    else
		    {
			g_message("Received XI Media Config service");
			if (update_gwylist(gwydata)==FALSE )
			{
			    g_message("Failed to update gw data into the list\n");
			    g_critical("Unable to update the Client device-%s in the list",gwydata->serial_num);
			    return FALSE;
			}
		    }
                }
                else
		{
                    g_message("Client device receiver id is NULL");
		}
            }
        }
        else
	{
            g_message("Client UDN is NULL");
	}
    }

    g_free(sno);
    deviceAddNo--;
    g_message("Exting from Device_proxy_available_cb client deviceAddNo = %u",deviceAddNo);

}

static void
device_proxy_available_cb_gw (GUPnPControlPoint *cp, GUPnPDeviceProxy *dproxy)
{
    g_message("In device_proxy_available_cb_gw found a Gateway device. deviceAddNo = %u ",deviceAddNo);
    deviceAddNo++;
    if ((NULL==cp) || (NULL==dproxy))
    {
        deviceAddNo--;
        g_message("WARNING - Received a null pointer for gateway device device no %u",deviceAddNo);
        return;
    }
    GwyDeviceData *gwydata = g_new(GwyDeviceData,1);
    init_gwydata(gwydata);
    gchar* sno = gupnp_device_info_get_serial_number (GUPNP_DEVICE_INFO (dproxy));
    GList* xdevlistitem = g_list_find_custom(xdevlist,sno,(GCompareFunc)g_list_find_sno);
    if(xdevlistitem!=NULL)
    {
        deviceAddNo--;
        g_message("Existing available_cb_gw as SNO is present in list so no update of devices %s device no %u",sno,deviceAddNo);
	g_free(sno);
        return;
    }
    gwydata->sproxy_i = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), XDISC_SERVICE_IDENTITY);
    gwydata->sproxy_m = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), XDISC_SERVICE_MEDIA);
    gwydata->sproxy_t = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), XDISC_SERVICE_TIME);
    gwydata->sproxy_g = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), XDISC_SERVICE_GW_CFG);
    gwydata->sproxy_q = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), XDISC_SERVICE_QAM_CFG);
    if (sno != NULL)
    {
        g_message("In available_cb_gw Gateway device serial number: %s",sno);
        g_string_assign(gwydata->serial_num, sno);
        const char* udn = gupnp_device_info_get_udn(GUPNP_DEVICE_INFO (dproxy));
        if (udn != NULL)
        {
            if (g_strrstr(udn,"uuid:"))
            {
                gchar* receiverid = g_strndup(udn+5, strlen(udn)-5);
                if(receiverid)
                {
                    g_message("Gateway device receiver id is %s",receiverid);
                    g_string_assign(gwydata->receiverid, receiverid);
                    g_free(receiverid);
                    if(!process_gw_services_identity(gwydata->sproxy_i, gwydata))
                    {
                        free_gwydata(gwydata);
                        g_free(gwydata);
                        g_free(sno);
                        deviceAddNo--;
                        g_message("Exting from available_cb_gw since mandatory paramters are not there  device no %u",deviceAddNo);
                        return;
                    }
                    else
                    {
			g_message("Received XI Identity service");
                    }
                    if(!process_gw_services_media_config(gwydata->sproxy_m, gwydata))
                    {
                        free_gwydata(gwydata);
                        g_free(gwydata);
                        g_free(sno);
                        deviceAddNo--;
                        g_message("Exting from available_cb_gw since mandatory paramters are not there  device no %u",deviceAddNo);
                        return;
                    }
		    else
                    {
			g_message("Received XI Media Config service");
                    }
                    if(!process_gw_services_time_config(gwydata->sproxy_t, gwydata))
                    {
                        free_gwydata(gwydata);
                        g_free(gwydata);
                        g_free(sno);
                        deviceAddNo--;
                        g_message("Exting from available_cb_gw since mandatory paramters are not there  device no %u",deviceAddNo);
                        return;
                    }
		    else
                    {
			g_message("Received XI Time Config service");
                    }
                    if(!process_gw_services_gateway_config(gwydata->sproxy_g, gwydata))
                    {
                        free_gwydata(gwydata);
                        g_free(gwydata);
                        g_free(sno);
                        deviceAddNo--;
                        g_message("Exting from available_cb_gw since mandatory paramters are not there  device no %u",deviceAddNo);
                        return;
                    }
		    else
                    {
			g_message("Received XI Gateway Config service");
                    }
                    if(!process_gw_services_qam_config(gwydata->sproxy_q, gwydata))
		    {
		        free_gwydata(gwydata);
		        g_free(gwydata);
                        g_free(sno);
                        deviceAddNo--;
                        g_message("Exting from available_cb_gw since mandatory paramters are not there  device no %u",deviceAddNo);
                        return;
                    }
                    else
                    {
			g_message("Received XI QAM Config service");
                        if (update_gwylist(gwydata)==FALSE )
                        {
                            g_message("Failed to update gw data into the list\n");
                            g_critical("Unable to update the gateway-%s in the device list",gwydata->serial_num);
                            return FALSE;
                        }
                    }
                }
                else
		{
                    g_message("In available_cb_gw gateway device receiver id is NULL");
		}
            }
        }
        else
	{
            g_message("Gateway UDN is NULL");
	}
    }
    if(g_strrstr(g_strstrip(gwydata->devicetype->str),"XI") == NULL )
    {
       if (gupnp_service_proxy_add_notify (gwydata->sproxy_m, "PlaybackUrl", G_TYPE_STRING, on_last_change, NULL) == FALSE)
           g_message("Failed to add url notifications for %s", sno);
       if (gupnp_service_proxy_add_notify (gwydata->sproxy_q, "SystemIds", G_TYPE_STRING, on_last_change, NULL) == FALSE)
           g_message("Failed to add systemid notifications for %s", sno);
       if (gupnp_service_proxy_add_notify (gwydata->sproxy_g, "DnsConfig", G_TYPE_STRING, on_last_change, NULL) == FALSE)
           g_message("Failed to add DNS notifications for %s", sno);
       if (gupnp_service_proxy_add_notify (gwydata->sproxy_t, "TimeZone", G_TYPE_STRING, on_last_change, NULL) == FALSE)
           g_message("Failed to add TimeZone notifications for %s", sno);
       if (gupnp_service_proxy_add_notify (gwydata->sproxy_q, "VideoBaseUrl", G_TYPE_STRING, on_last_change, NULL) == FALSE)
           g_message("Failed to add VideoBaseUrl notifications for %s", sno);
    }
    gupnp_service_proxy_set_subscribed(gwydata->sproxy_m, TRUE);
    gupnp_service_proxy_set_subscribed(gwydata->sproxy_t, TRUE);
    gupnp_service_proxy_set_subscribed(gwydata->sproxy_g, TRUE);
    gupnp_service_proxy_set_subscribed(gwydata->sproxy_q, TRUE);

    if (gupnp_service_proxy_add_notify (gwydata->sproxy_m, "FogTsbUrl", G_TYPE_STRING, on_last_change, NULL) == FALSE)
           g_message("Failed to add FogTsbUrl notifications for %s", sno);

    if ((gupnp_service_proxy_get_subscribed(gwydata->sproxy_m) == FALSE) && (gupnp_service_proxy_get_subscribed(gwydata->sproxy_t) == FALSE) && (gupnp_service_proxy_get_subscribed(gwydata->sproxy_g) == FALSE) && (gupnp_service_proxy_get_subscribed(gwydata->sproxy_q) == FALSE))
    {
        g_message("Failed to register for notifications on %s", sno);
        g_object_unref(gwydata->sproxy_m);
        g_object_unref(gwydata->sproxy_t);
        g_object_unref(gwydata->sproxy_g);
        g_object_unref(gwydata->sproxy_q);
    }
    g_free(sno);
    deviceAddNo--;
    g_message("Exting from device_proxy_available_cb_gateway deviceAddNo = %u",deviceAddNo);
}

static void
device_proxy_unavailable_cb_bgw (GUPnPControlPoint *cp, GUPnPDeviceProxy *dproxy)
{
    const gchar* sno = gupnp_device_info_get_serial_number (GUPNP_DEVICE_INFO (dproxy));
    g_message ("In unavailable_bgw Device %s went down",sno);
    if (delete_gwyitem(sno) == FALSE)
    {
        g_message("%s found, but unable to delete it from list", sno);
        return FALSE;
    }
    else
    {
        g_message("Deleted %s from list", sno);
        sendDiscoveryResult(disConf->outputJsonFile);
    }
    g_free(sno);
}

static void
device_proxy_available_cb_bgw (GUPnPControlPoint *cp, GUPnPDeviceProxy *dproxy)
{
    g_message("In available_bgw found a Broadband device. deviceAddNo = %u ",deviceAddNo);
    deviceAddNo++;
    if ((NULL==cp) || (NULL==dproxy))
    {
        deviceAddNo--;
        g_message("WARNING - Received a null pointer for broadband device device no %u",deviceAddNo);
        return;
    }
    gchar* sno = gupnp_device_info_get_serial_number (GUPNP_DEVICE_INFO (dproxy));
    GList* xdevlistitem = g_list_find_custom(xdevlist,sno,(GCompareFunc)g_list_find_sno);
    if(xdevlistitem!=NULL)
    {
        deviceAddNo--;
        g_message("Existing available_cb_bgw as SNO is present in list so no update of devices %s device no %u",sno,deviceAddNo);
	g_free(sno);
        return;
    }
    GwyDeviceData *gwydata = g_new(GwyDeviceData,1);
    init_gwydata(gwydata);
    gwydata->sproxy_i = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), XDISC_SERVICE_IDENTITY);
    gwydata->sproxy_t = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), XDISC_SERVICE_TIME);
    gwydata->sproxy_g = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), XDISC_SERVICE_GW_CFG);
    if (sno != NULL)
    {
        g_message("In available_cb_bgw Broadband device serial number: %s",sno);
        g_string_assign(gwydata->serial_num, sno);
        const char* udn = gupnp_device_info_get_udn(GUPNP_DEVICE_INFO (dproxy));
        if (udn != NULL)
        {
            if (g_strrstr(udn,"uuid:"))
            {
                gchar* receiverid = g_strndup(udn+5, strlen(udn)-5);
                if(receiverid)
                {
                    g_message("Gateway device receiver id is %s",receiverid);
                    g_string_assign(gwydata->receiverid, receiverid);
                    g_free(receiverid);
                    if(!process_gw_services_identity(gwydata->sproxy_i, gwydata))
                    {
                        free_gwydata(gwydata);
                        g_free(gwydata);
                        g_free(sno);
                        deviceAddNo--;
                        g_message("Exting from available_cb_bgw since Identity paramters are not there  device no %u",deviceAddNo);
                        return;
                    }
                    else
                    {
                        g_message("Received XI Identity service");
                    }
                    if(!process_gw_services_time_config(gwydata->sproxy_t, gwydata))
                    {
                        free_gwydata(gwydata);
                        g_free(gwydata);
                        g_free(sno);
                        deviceAddNo--;
                        g_message("Exting from available_cb_bgw since Time paramters are not there  device no %u",deviceAddNo);
                        return;
                    }
                    else
                    {
                        g_message("Received XI Time Config service");
                    }
                    if(!process_gw_services_gateway_config(gwydata->sproxy_g, gwydata))
                    {
                        free_gwydata(gwydata);
                        g_free(gwydata);
                        g_free(sno);
                        deviceAddNo--;
                        g_message("Exting from available_cb_bgw since mandatory paramters are not there  device no %u",deviceAddNo);
                        return;
                    }
                    else
                    {
                        g_message("Received XI Time Config service");
                        if (update_gwylist(gwydata)==FALSE )
                        {
                            g_message("Failed to update gw data into the list\n");
                            g_critical("Unable to update the gateway-%s in the device list",gwydata->serial_num);
                            return FALSE;
                        }
                    }
                }
                else
                {
                    g_message("In available_cb_bgw gateway device receiver id is NULL");
                }
            }
        }
        else
        {
            g_message("Gateway UDN is NULL");
        }
    }

    gupnp_service_proxy_set_subscribed(gwydata->sproxy_i, TRUE);
    gupnp_service_proxy_set_subscribed(gwydata->sproxy_t, TRUE);
    gupnp_service_proxy_set_subscribed(gwydata->sproxy_g, TRUE);

    if ((gupnp_service_proxy_get_subscribed(gwydata->sproxy_i) == FALSE) && (gupnp_service_proxy_get_subscribed(gwydata->sproxy_t) == FALSE) && (gupnp_service_proxy_get_subscribed(gwydata->sproxy_g) == FALSE))
    {
            g_message("Failed to register for notifications on %s", sno);
            g_object_unref(gwydata->sproxy_i);
            g_object_unref(gwydata->sproxy_t);
            g_object_unref(gwydata->sproxy_g);
    }
    g_free(sno);
    deviceAddNo--;
    g_message("Exting from device_proxy_available_cb_broadband deviceAddNo = %u",deviceAddNo);
}

int main(int argc, char *argv[])
{
    //preamble
    g_thread_init (NULL);
    g_type_init();
//#ifndef GUPNP_0_19
//	GMainContext *main_context;
//#endif

    //GSource* time_source;
    gboolean ipv6Enabled=FALSE;
    gboolean bInterfaceReady=FALSE;

    GError* error = 0;
    GThread *thread;
    //int id;
    mutex = g_mutex_new ();
    devMutex = g_mutex_new ();
    cond = g_cond_new ();
    logoutfile = NULL;

#ifdef ENABLE_BREAKPAD
    installExceptionHandler();
#endif
#ifdef BROADBAND
    breakpad_ExceptionHandler();
#endif
//   outputfilename = g_string_new("null");
    outputcontents = g_string_new(NULL);
    ipMode = g_string_new("ipv4");

    ownSerialNo=g_string_new(NULL);
    if (argc < 2)
    {
        fprintf(stderr, "Error in arguments\nUsage: %s config file name (eg: /etc/xdiscovery.conf) %s log file location \n", argv[0], argv[1]);
        exit(1);
    }


//    outputfilename = g_string_assign(outputfilename, argv[2]);
//    const char* ifname = argv[1];

    char* logfilename = argv[2];
    if(logfilename)
    {
        logoutfile = g_fopen (logfilename, "a");
    }
    /*else if (disConf->logFile)
    {
        logoutfile = g_fopen (disConf->logFile, "a");
    }*/
    else
    {
        g_message("xupnp not handling the logging");
    }
    if(logoutfile)
    {
        g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_INFO | G_LOG_LEVEL_MESSAGE | \
                          G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_ERROR, xupnp_logger, NULL);
    }
    rfc_enabled = check_rfc();
    if(!rfc_enabled)
    {
        g_message("Running Older Xdiscovery");
    }
    char* configfilename = argv[1];
    if (readconffile(configfilename)==FALSE)
    {
        g_message("Unable to find xdevice config, Loading default settings \n");
//        disConf->discIf = g_string_assign(disConf->discIf, "eth1");
//        disConf->gwIf = g_string_assign(disConf->gwIf, "eth1");
//        disConf->gwSetupFile = g_string_assign(disConf->gwSetupFile, "/lib/rdk/gwSetup.sh");
//        disConf->logFile = g_string_assign(disConf->logFile, "/opt/logs/xdiscovery.log");
//        disConf->outputJsonFile = g_string_assign(disConf->outputJsonFile, "/opt/output.json");
//            disConf->GwPriority = 500
//              disConf->enableGwSetup = false
        exit(1);
    }

    /* int result = getipaddress(disConf->discIf,ipaddress,ipv6Enabled);

    if (!result)
    {
    	fprintf(stderr,"Could not locate the ipaddress of the discovery interface %s\n", disConf->discIf);
    	g_critical("Could not locate the ipaddress of the discovery interface %s\n", disConf->discIf);
    	exit(1);
    }
    	else
    		g_message("ipaddress of the interface %s\n", ipaddress);
    */
    gchar **tokens = g_strsplit_set(disConf->discIf,",", -1);
    guint toklength = g_strv_length(tokens);
    guint loopvar=0;
    for (loopvar=0; loopvar<toklength; loopvar++)
    {
        if ((strlen(g_strstrip(tokens[loopvar])) > 0))
        {
            int result = getipaddress(g_strstrip(tokens[loopvar]),ipaddress,ipv6Enabled);
            if (result)
            {
                g_message(" \n ***Got Ip address for interface  = %s ipaddr = %s *****",g_strstrip(tokens[loopvar]),ipaddress);
                g_stpcpy(disConf->discIf,g_strstrip(tokens[loopvar]));
                g_stpcpy(disConf->gwIf,g_strstrip(tokens[loopvar]));
                bInterfaceReady=TRUE;
                break;
            }
            else
            {
                g_message("Could not locate the ipaddress of the broadcast interface %s continue with next \n", g_strstrip(tokens[loopvar]));
                bInterfaceReady=FALSE;
            }

        }
    }
    g_strfreev(tokens);
    if ( FALSE == bInterfaceReady)
    {

        fprintf(stderr,"Could not locate the ipaddress of the discovery interface %s\n", disConf->discIf);
        g_critical("Could not locate the ipaddress of the discovery interface %s\n", disConf->discIf);
        exit(1);
    }

    g_message("Starting xdiscovery service on interface %s", disConf->discIf);
    g_print("Starting xdiscovery service on interface %s\n", disConf->discIf);


#ifdef GUPNP_0_19
    context = gupnp_context_new (NULL, disConf->discIf, host_port, &error);
#else
    main_context = g_main_context_new();
    context = gupnp_context_new (main_context, disConf->discIf, host_port, &error);
#endif

    if (error) {
        g_message ("Error creating the GUPnP context: %s\n", error->message);
        g_critical("Error creating the XUPnP context on %s:%d Error:%s", disConf->discIf, host_port, error->message);
        g_error_free (error);
        return EXIT_FAILURE;
    }

#if defined(USE_XUPNP_IARM) || defined(USE_XUPNP_IARM_BUS)
    gboolean iarminit = XUPnP_IARM_Init();
    if (iarminit==true)
    {
        //g_print("IARM init success");
        g_message("XUPNP IARM init success");
    }
    else
    {
        //g_print("IARM init failure");
        g_critical("XUPNP IARM init failed");
    }
    broadcastIPModeChange();
#endif
    //context = gupnp_context_new (main_context, host_ip, host_port, NULL);
    gupnp_context_set_subscription_timeout(context, 0);
    if(rfc_enabled)
    {
        g_message("RFC enabled for new device refactoring");
        upnpContextDeviceProtect = gupnp_context_new (NULL, disConf->discIf, DEVICE_PROTECTION_CONTEXT_PORT, &error);
        if (error) {
            g_printerr ("Error creating the Device Protection Broadcast context: %s\n",
                        error->message);
            /* g_clear_error() frees the GError *error memory and reset pointer if set in above operation */
            g_clear_error(&error);
            return EXIT_FAILURE;
        }
        gupnp_context_set_subscription_timeout(upnpContextDeviceProtect, 0);
        cp_client = gupnp_control_point_new(upnpContextDeviceProtect, "urn:schemas-upnp-org:device:X1Renderer:1");
        g_signal_connect (cp_client,"device-proxy-available", G_CALLBACK (device_proxy_available_cb_client), NULL);
        g_signal_connect (cp_client,"device-proxy-unavailable", G_CALLBACK (device_proxy_unavailable_cb_client), NULL);
        gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp_client), TRUE);

        cp_gw = gupnp_control_point_new(upnpContextDeviceProtect, "urn:schemas-upnp-org:device:X1VideoGateway:1");
        g_signal_connect (cp_gw,"device-proxy-available", G_CALLBACK (device_proxy_available_cb_gw), NULL);
        g_signal_connect (cp_gw,"device-proxy-unavailable", G_CALLBACK (device_proxy_unavailable_cb_gw), NULL);
        gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp_gw), TRUE);

        cp_bgw = gupnp_control_point_new(upnpContextDeviceProtect, "urn:schemas-upnp-org:device:X1BroadbandGateway:1");
        g_signal_connect (cp_bgw,"device-proxy-available", G_CALLBACK (device_proxy_available_cb_bgw), NULL);
        g_signal_connect (cp_bgw,"device-proxy-unavailable", G_CALLBACK (device_proxy_unavailable_cb_bgw), NULL);
        gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp_bgw), TRUE);
    }

    cp = gupnp_control_point_new(context, "urn:schemas-upnp-org:device:BasicDevice:1");
    g_signal_connect (cp,"device-proxy-available", G_CALLBACK (device_proxy_available_cb), NULL);
    g_signal_connect (cp,"device-proxy-unavailable", G_CALLBACK (device_proxy_unavailable_cb), NULL);
    gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

    logMilestone("UPNP_START_DISCOVERY");
#ifdef GUPNP_0_19
    main_loop = g_main_loop_new (NULL, FALSE);
#else
    main_loop = g_main_loop_new (main_context, FALSE);
#endif

    thread = g_thread_create(verify_devices, NULL,FALSE, NULL);
    g_message("Started discovery on interface %s", disConf->discIf);
    g_main_loop_run (main_loop);
    g_main_loop_unref (main_loop);
    g_object_unref (cp);
    if(rfc_enabled)
    {
        g_object_unref (cp_client);
        g_object_unref (cp_gw);
        g_object_unref (cp_bgw);
        g_object_unref (upnpContextDeviceProtect);
    }
    g_object_unref (context);
    if(logoutfile)
    {
        fclose (logoutfile);
    }
    return 0;
}

/**
 * @brief This function is used to get attributes of different Gateway services using GUPnP Service Proxy.
 *
 * GUPnP Service Proxy sends commands to a remote UPnP service and handles incoming event notifications.
 * It will request the proxy to get services and attributes such as:
 * - Base TRM URL.
 * - Playback URL.
 * - Gateway IP.
 * - DNS Configuration.
 * - Device Type.
 * - Time Zone.
 * - DST (Daylight Saving Time).
 * - MAC Address.
 * - System ID and so on.
 *
 * @param[in] sproxy Address of the GUPnP Service Proxy instance.
 * @param[in] gwData Address of the Structure holding gateway service attributes.
 *
 * @return Returns TRUE if the gateway service requests processed successfully else returns FALSE.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
gboolean process_gw_services(GUPnPServiceProxy *sproxy, GwyDeviceData* gwData)
{
    /* Does not look elegant, but call all the actions one after one - Seems like
     UPnP service proxy does not provide a way to call all the actions in a list
    */
    GError *error = NULL;
    gchar *temp=NULL;
    guint *temp_i=NULL;
    gboolean *temp_b=NULL;

    g_message("Entering into process_gw_services ");

    gupnp_service_proxy_send_action (sproxy, "GetIsGateway", &error, NULL,"IsGateway", G_TYPE_BOOLEAN, &temp_b ,NULL);
    if (error!=NULL)
    {
        g_message ("GetIsGateway process gw services  Error: %s\n", error->message);
        g_message("TELEMETRY_XUPNP_PARTIAL_DISCOVERY:%d,IsGateway",error->code);
        g_clear_error(&error);
        return FALSE;
    }
    else
    {
        gwData->isgateway = temp_b;
    }

    gupnp_service_proxy_send_action (sproxy, "GetRecvDevType", &error,NULL,"RecvDevType",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message("GetRecvDevType process gw services Error: %s\n", error->message);
        g_clear_error(&error);
//        return FALSE;
    }
    else
    {
        g_string_assign(gwData->recvdevtype,temp);
        g_free(temp);
    }

    gupnp_service_proxy_send_action (sproxy, "GetBuildVersion", &error,NULL,"BuildVersion",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message("GetBuildVersion process gw services Error: %s\n", error->message);
        g_clear_error(&error);
//        return FALSE;
    }
    else
    {
        g_string_assign(gwData->buildversion,temp);
        g_free(temp);
    }

    gupnp_service_proxy_send_action (sproxy, "GetDeviceName", &error,NULL,"DeviceName",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetDeviceName process gw services Error: %s\n", error->message);
        g_clear_error(&error);
//        return FALSE;
    }
    else
    {
        g_string_assign(gwData->devicename,temp);
        g_free(temp);
    }

    gupnp_service_proxy_send_action (sproxy, "GetDeviceType", &error,NULL,"DeviceType",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetDeviceType process gw services Error: %s\n", error->message);
        g_clear_error(&error);
//        return FALSE;
    }
    else
    {
        g_string_assign(gwData->devicetype,temp);
        g_free(temp);
    }

    gupnp_service_proxy_send_action (sproxy, "GetBcastMacAddress", &error,NULL,"BcastMacAddress",G_TYPE_STRING, &temp,NULL);
    if (error!=NULL)
    {
        g_message (" GetBcastMacAddress process gw services Error: %s\n", error->message);
        g_message("TELEMETRY_XUPNP_PARTIAL_DISCOVERY:%d,BcastMacAddress",error->code);
        g_clear_error(&error);
        return FALSE;
    }
    else
    {
        g_string_assign(gwData->bcastmacaddress,temp);
        g_free(temp);
    }

    if(g_strrstr(g_strstrip(gwData->devicetype->str),"XI") == NULL )
    {
        g_message("Discovered Device is XG or RNG Device ");

    gupnp_service_proxy_send_action (sproxy, "GetBaseUrl", &error,NULL,"BaseUrl",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetBaseUrl process gw services Error: %s\n", error->message);
        g_message("TELEMETRY_XUPNP_PARTIAL_DISCOVERY:%d,BaseUrl",error->code);
//        g_critical("Failed to get BaseUrl from gateway-%s: Error %s",gwData->serial_num, error->message);
        g_clear_error(&error);
        return FALSE;
    }
    else
    {
        g_string_assign(gwData->baseurl,temp);
        g_free(temp);
    }

    //Do not return error as the boxes which are not running trm supported code base could still exist in the network
    gupnp_service_proxy_send_action (sproxy, "GetBaseTrmUrl", &error, NULL,"BaseTrmUrl",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetBaseTrmUrl process gw services Error: %s\n", error->message);
        g_message("TELEMETRY_XUPNP_PARTIAL_DISCOVERY:%d,BaseTrmUrl",error->code);
        g_clear_error(&error);
        return FALSE;
    }
    else
    {
        g_string_assign(gwData->basetrmurl,temp);
        g_free(temp);
    }
    //Do not return error as the boxes which are not running single step tuning code base could still exist in the network

    gupnp_service_proxy_send_action (sproxy, "GetGatewayIP", &error,NULL,"GatewayIP",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetGatewayIP process gw services Error: %s\n", error->message);
//        g_critical("Failed to get GatewayIP from gateway-%s: Error: %s",gwData->serial_num, error->message);
        g_clear_error(&error);
//        return FALSE;
    }
    else
    {
        g_string_assign(gwData->gwyip,temp);
        g_free(temp);
    }

    gupnp_service_proxy_send_action (sproxy, "GetGatewayIPv6", &error,NULL,"GatewayIPv6",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetGatewayIPv6 process gw services Error: %s\n", error->message);
//        g_critical("Failed to get GatewayIPv6 from gateway-%s: Error: %s",gwData->serial_num, error->message);
        g_clear_error(&error);
//        return FALSE;
    }
    else
    {
        g_string_assign(gwData->gwyipv6,temp);
        g_free(temp);
    }

    gupnp_service_proxy_send_action (sproxy, "GetDnsConfig", &error,NULL,"DnsConfig",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetDnsConfig process gw services Error: %s\n", error->message);
//        g_critical("Failed to get DnsConfig from gateway-%s: Error: %s",gwData->serial_num, error->message);
        g_clear_error(&error);
//        return FALSE;
    }
    else
    {
        g_string_assign(gwData->dnsconfig,temp);
        g_free(temp);
    }

    gupnp_service_proxy_send_action (sproxy, "GetGatewayStbIP", &error,NULL,"GatewayStbIP",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetGatewayStbIP process gw services Error: %s\n", error->message);
        g_clear_error(&error);
//        return FALSE;
    }
    else
    {
        g_string_assign(gwData->gatewaystbip,temp);
        g_free(temp);
    }

    gupnp_service_proxy_send_action (sproxy, "GetIpv6Prefix", &error,NULL,"Ipv6Prefix",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetIpv6Prefix process gw services Error: %s\n", error->message);
        g_clear_error(&error);
//        return FALSE;
    }
    else 
    {
	g_string_assign(gwData->ipv6prefix,temp);
	g_free(temp);
        if (gwData && gwData->ipv6prefix && gwData->ipv6prefix->str && (g_strcmp0(gwData->ipv6prefix->str, "") != 0))
        {
            logMilestone("UPNP_RECV_IPV6_PREFIX");
        }
    }

    gupnp_service_proxy_send_action (sproxy, "GetPlaybackUrl", &error,NULL,"PlaybackUrl",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetPlaybackUrl process gw services Error: %s\n", error->message);
        g_message("TELEMETRY_XUPNP_PARTIAL_DISCOVERY:%d,PlaybackUrl",error->code);
        g_clear_error(&error);
        return FALSE;
    }
    else
    {
        g_string_assign(gwData->playbackurl,temp);
        g_free(temp);
    }
    g_message("GetPlaybackUrl = %s",gwData->playbackurl->str);

    gupnp_service_proxy_send_action (sproxy, "GetSystemIds", &error,NULL,"SystemIds",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetSystemIds process gw services Error: %s\n", error->message);
    //    g_critical("Failed to get SystemIds from gateway-%s: Error: %s",gwData->serial_num, error->message);
        g_clear_error(&error);
//        return FALSE;
    }
    else
    {
        g_string_assign(gwData->systemids,temp);
        g_free(temp);
    }

    gupnp_service_proxy_send_action (sproxy, "GetTimeZone", &error,NULL,"TimeZone",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetTimeZone process gw services Error: %s\n", error->message);
//        g_critical("Failed to get TimeZone from gateway-%s: Error: %s",gwData->serial_num, error->message);
        g_clear_error(&error);
//        return FALSE;
    }
    else
    {
        g_string_assign(gwData->dsgtimezone,temp);
        g_free(temp);
    }

    gupnp_service_proxy_send_action (sproxy, "GetHosts", &error,NULL,"Hosts",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetHosts process gw services Error: %s\n", error->message);
//        g_critical("Failed to get Hosts from gateway-%s: Error: %s",gwData->serial_num, error->message);
        g_clear_error(&error);
//        return FALSE;
    }
    else
    {
        g_string_assign(gwData->etchosts,temp);
        g_free(temp);
    }

    //Do not return error as the boxes which are not running trm supported code base could still exist in the network
    gupnp_service_proxy_send_action (sproxy, "GetRequiresTRM", &error,NULL,"RequiresTRM",G_TYPE_BOOLEAN, &temp_b ,NULL);
    if (error!=NULL)
    {
        g_message ("Error: %s\n", error->message);
        g_message("TELEMETRY_XUPNP_PARTIAL_DISCOVERY:%d,RequiresTRM",error->code);
        g_clear_error(&error);
        return FALSE;
    }
    else
    {
        gwData->requirestrm = temp_b;
    }

    gupnp_service_proxy_send_action (sproxy, "GetHostMacAddress", &error,NULL,"HostMacAddress",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message("GetHostMacAddress process gw services Error: %s\n", error->message);
        g_clear_error(&error);
//        return FALSE;
    }
    else
    {
        g_string_assign(gwData->hostmacaddress,temp);
        g_free(temp);
    }

    gupnp_service_proxy_send_action (sproxy, "GetRawOffSet", &error,NULL,"RawOffSet",G_TYPE_INT, &temp_i,NULL);
    if (error!=NULL)
    {
        g_message ("GetRawOffSet process gw services Error: %s\n", error->message);
        g_clear_error(&error);
//        g_critical("Failed to get rawOffset from gateway-%s: Error: %s",gwData->serial_num, error->message);
    }
    else
    {
        gwData->rawoffset = temp_i;
    }

    gupnp_service_proxy_send_action (sproxy, "GetDSTSavings", &error,NULL,"DSTSavings",G_TYPE_INT,&temp_i,NULL);
    if (error!=NULL)
    {
        g_message ("GetDSTSavings process gw services Error: %s\n", error->message);
        g_clear_error(&error);
//        g_critical("Failed to get dstSavings from gateway-%s: Error: %s",gwData->serial_num, error->message);
    }
    else
    {
        gwData->dstsavings = temp_i;
    }

    gupnp_service_proxy_send_action (sproxy, "GetUsesDaylightTime", &error,NULL,"UsesDaylightTime",G_TYPE_BOOLEAN, &temp_b,NULL);
    if (error!=NULL)
    {
        g_message ("GetUsesDaylightTime process gw services Error: %s\n", error->message);
        g_clear_error(&error);
//        g_critical("Failed to get usesDaylightTime from gateway-%s: Error: %s",gwData->serial_num, error->message);
    }
    else
    {
        gwData->usesdaylighttime = temp_b;
    }

    gupnp_service_proxy_send_action (sproxy, "GetDSTOffset", &error,NULL,"DSTOffset",G_TYPE_INT, &temp_i,NULL);
    if (error!=NULL)
    {
        g_message ("GetDSTOffset process gw services Error: %s\n", error->message);
        g_clear_error(&error);
//        g_critical("Failed to get DSTOffset from gateway-%s: Error: %s",gwData->serial_num, error->message);
    }
    else
    {
        gwData->dstoffset = temp_i;
    }
    }
    else
    {
        g_message("Discovered Device is Xi Device ");

    gupnp_service_proxy_send_action (sproxy, "GetDataGatewayIPaddress",&error,NULL,"DataGatewayIPaddress",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetDataGatewayIPaddress process gw services Error: %s\n", error->message);
        g_clear_error(&error);
        //return FALSE;
    }
    else
    {
        g_string_assign(gwData->dataGatewayIPaddress,temp);
        g_free(temp);
    }
    g_message("GetDataGatewayIPaddress = %s",gwData->dataGatewayIPaddress->str);
    }
#ifndef CLIENT_XCAL_SERVER

    //if ((gwData->isgateway == TRUE) && (gwData->isOwnGateway == FALSE))
    if (gwData->isOwnGateway == FALSE)
    {
        if (replace_local_device_ip(gwData) == TRUE)
        {
            g_message("Replaced MocaIP for %s with localhost", gwData->serial_num->str);
        }
    }
#endif
    gwData->devFoundFlag = TRUE;

    if (update_gwylist(gwData)==FALSE )
    {
        g_message("Failed to update gw data into the list\n");
        g_critical("Unable to update the gateway-%s in the device list",gwData->serial_num);
        return FALSE;
    }
    g_message("Exiting from process_gw_services ");
    return TRUE;
}

gboolean process_gw_services_identity(GUPnPServiceProxy *sproxy, GwyDeviceData* gwData)
{
    /* Does not look elegant, but call all the actions one after one - Seems like
     UPnP service proxy does not provide a way to call all the actions in a list
    */
    GError *error = NULL;
    gchar *temp=NULL;

    g_message("Entering into process_gw_services_identity ");
    gupnp_service_proxy_send_action (sproxy, "GetRecvDevType", &error,NULL,"RecvDevType",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message("GetRecvDevType process gw Identity services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->recvdevtype, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetBuildVersion", &error,NULL,"BuildVersion",G_TYPE_STRING, &temp  ,NULL);
    if (error!=NULL)
    {
        g_message("GetBuildVersion process gw Identity services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->buildversion, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetDeviceName", &error,NULL,"DeviceName",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetDeviceName process gw Identity services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->devicename, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetDeviceType", &error,NULL,"DeviceType",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetDeviceType process gw Identity services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->devicetype, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetModelClass", &error,NULL,"ModelClass",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetModelClass process gw Identity services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->modelclass, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetModelNumber", &error,NULL,"ModelNumber",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetModelNumber process gw Identity services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->modelnumber, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetDeviceId", &error,NULL,"DeviceId",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetDeviceId process gw Identity services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->deviceid, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetHardwareRevision", &error,NULL,"HardwareRevision",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetHardwareRevision process gw Identity services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->hardwarerevision, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetSoftwareRevision", &error,NULL,"SoftwareRevision",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetSoftwareRevision process gw Identity services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->softwarerevision, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetManagementUrl", &error,NULL,"ManagementURL",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetManagementUrl process gw Identity services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->managementurl, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetMake", &error,NULL,"Make",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetMake process gw Identity services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->make, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetAccountId", &error,NULL,"AccountId",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetAccountId process gw Identity services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->accountid, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetReceiverId", &error,NULL,"ReceiverId",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetReceiverId process gw Identity services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->receiverid, temp);
        g_free(temp);
    }
    g_message("Exiting from process_gw_services_identity ");
    return TRUE;
}


gboolean process_gw_services_media_config(GUPnPServiceProxy *sproxy, GwyDeviceData* gwData)
{
    /* Does not look elegant, but call all the actions one after one - Seems like
     UPnP service proxy does not provide a way to call all the actions in a list
    */
    GError *error = NULL;
    gchar *temp=NULL;

    g_message("Entering into process_gw services_media_config ");
    gupnp_service_proxy_send_action (sproxy, "GetPlaybackUrl", &error,NULL,"PlaybackUrl",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetPlaybackUrl process gw media services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->playbackurl, temp);
        g_free(temp);
    }
/*    gupnp_service_proxy_send_action (sproxy, "GetFogTsbUrl",&error,NULL,"FogTsbUrl",G_TYPE_STRING, gwData->fogtsburl ,NULL);
    if (error!=NULL)
    {
        g_message (" GetFogTsbUrl process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }*/
    g_message("Exiting from process_gw_services_media_config ");
    return TRUE;
}

gboolean process_gw_services_gateway_config(GUPnPServiceProxy *sproxy, GwyDeviceData* gwData)
{
    /* Does not look elegant, but call all the actions one after one - Seems like
     UPnP service proxy does not provide a way to call all the actions in a list
    */
    GError *error = NULL;
    gchar *temp=NULL;
    gboolean *temp_b=NULL;

    g_message("Entering into process_gw_services_gateway_config ");
    gupnp_service_proxy_send_action (sproxy, "GetDataGatewayIPaddress",&error,NULL,"DataGatewayIPaddress",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetDataGatewayIPaddress process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->dataGatewayIPaddress, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetGatewayStbIP", &error,NULL,"GatewayStbIP",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetGatewayStbIP process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->gatewaystbip, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetIpv6Prefix", &error,NULL,"Ipv6Prefix",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetIpv6Prefix process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->ipv6prefix, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetDeviceName", &error,NULL,"DeviceName",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetDeviceName process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->devicename, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetBcastMacAddress", &error,NULL,"BcastMacAddress",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetBcastMacAddress process gw services Error: %s\n", error->message);
        g_message("TELEMETRY_XUPNP_PARTIAL_DISCOVERY:%d,BcastMacAddress",error->code);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->bcastmacaddress, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetGatewayIPv6", &error,NULL,"GatewayIPv6",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetGatewayIPv6 process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->gwyipv6, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetGatewayIP", &error,NULL,"GatewayIP",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetGatewayIP process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->gwyip, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetHostMacAddress", &error,NULL,"HostMacAddress",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message("GetHostMacAddress process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->hostmacaddress, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetDnsConfig", &error,NULL,"DnsConfig",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message (" GetDnsConfig process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->dnsconfig, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetHosts", &error,NULL,"Hosts",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetHosts process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->etchosts, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetIsGateway", &error, NULL,"IsGateway", G_TYPE_BOOLEAN, &temp_b ,NULL);
    if (error!=NULL)
    {
        g_message ("GetIsGateway process gw services  Error: %s\n", error->message);
        g_message("TELEMETRY_XUPNP_PARTIAL_DISCOVERY:%d,IsGateway",error->code);
        g_clear_error(&error);
    }
    else
    {
        gwData->isgateway = temp_b;
    }

    g_message("Exiting from process_gw_services_gateway_config ");
    return TRUE;
}


gboolean process_gw_services_time_config(GUPnPServiceProxy *sproxy, GwyDeviceData* gwData)
{
    /* Does not look elegant, but call all the actions one after one - Seems like
     UPnP service proxy does not provide a way to call all the actions in a list
    */
    GError *error = NULL;
    gchar *temp=NULL;
    guint *temp_i=NULL;
    gboolean *temp_b=NULL;

    g_message("Entering into process_gw_services_time_config ");
    gupnp_service_proxy_send_action (sproxy, "GetTimeZone", &error,NULL,"TimeZone",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetTimeZone process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->dsgtimezone, temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetRawOffSet", &error,NULL,"RawOffSet",G_TYPE_INT, &temp_i ,NULL);
    if (error!=NULL)
    {
        g_message ("GetRawOffSet process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        gwData->rawoffset = temp_i;
    }
    gupnp_service_proxy_send_action (sproxy, "GetDSTSavings", &error,NULL,"DSTSavings",G_TYPE_INT, &temp_i ,NULL);
    if (error!=NULL)
    {
        g_message ("GetDSTSavings process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        gwData->dstsavings = temp_i;
    }
    gupnp_service_proxy_send_action (sproxy, "GetUsesDaylightTime", &error,NULL,"UsesDaylightTime",G_TYPE_BOOLEAN, &temp_b ,NULL);
    if (error!=NULL)
    {
        g_message ("GetUsesDaylightTime process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        gwData->usesdaylighttime = temp_b;
    }
    gupnp_service_proxy_send_action (sproxy, "GetDSTOffset", &error,NULL,"DSTOffset",G_TYPE_INT, &temp_i ,NULL);
    if (error!=NULL)
    {
        g_message ("GetDSTOffset process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        gwData->dstoffset = temp_i;
    }
    g_message("Exiting from process_gw_services_gateway_config ");
    return TRUE;
}

gboolean process_gw_services_qam_config(GUPnPServiceProxy *sproxy, GwyDeviceData* gwData)
{
    /* Does not look elegant, but call all the actions one after one - Seems like
     UPnP service proxy does not provide a way to call all the actions in a list
    */
    GError *error = NULL;
    gchar *temp=NULL;
    gboolean *temp_b=NULL;

    g_message("Entering into process_gw_services_qam_config ");

    //Do not return error as the boxes which are not running trm supported code base could still exist in the network
    gupnp_service_proxy_send_action (sproxy, "GetBaseTrmUrl", NULL,NULL,"BaseTrmUrl",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetBaseTrmUrl process gw services Error: %s\n", error->message);
        g_message("TELEMETRY_XUPNP_PARTIAL_DISCOVERY:%d,BaseTrmUrl",error->code);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->basetrmurl , temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetSystemIds", &error,NULL,"SystemIds",G_TYPE_STRING, &temp ,NULL);
    if (error!=NULL)
    {
        g_message ("GetSystemIds process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }
    else
    {
        g_string_assign(gwData->systemids , temp);
        g_free(temp);
    }
    gupnp_service_proxy_send_action (sproxy, "GetRequiresTRM", &error,NULL,"RequiresTRM",G_TYPE_BOOLEAN, &temp_b ,NULL);
    if (error!=NULL)
    {
        g_message ("Error: %s\n", error->message);
        g_message("TELEMETRY_XUPNP_PARTIAL_DISCOVERY:%d,RequiresTRM",error->code);
        g_clear_error(&error);
    }
    else
    {
        gwData->requirestrm = temp_b;
    }
/*    gupnp_service_proxy_send_action (sproxy, "GetVideoBaseUrl", &error,NULL,"VideoBaseUrl",G_TYPE_STRING, gwData->videobaseurl,NULL);
    if (error!=NULL)
    {
        g_message (" GetVideoBaseUrl process gw services Error: %s\n", error->message);
        g_clear_error(&error);
    }*/
    g_message("Exiting from process_gw_services_qam_config ");
    return TRUE;
}

/**
 * @brief Replace IP Address part of the Gateway URL attributes from home networking IP with the local host IP.
 * It Replaces the URL attribute of playbackurl, baseurl, basetrmurl.
 *
 * @param[in] gwyData Address of the structure holding Gateway attributes.
 *
 * @return Returns TRUE indicating the call has finished.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
gboolean replace_hn_with_local(GwyDeviceData* gwyData)
{
    //g_print("replace: %s with %s\n", gwyData->gwyip->str, localHostIP);
    g_string_assign(gwyData->playbackurl, replace_string(gwyData->playbackurl->str, gwyData->gwyip->str, localHostIP));
    g_string_assign(gwyData->baseurl, replace_string(gwyData->baseurl->str, gwyData->gwyip->str, localHostIP));
    g_string_assign(gwyData->basetrmurl, replace_string(gwyData->basetrmurl->str, gwyData->gwyip->str, localHostIP));
    g_message("baseurl:%s, playbackurl:%s, basetrumurl:%s", gwyData->baseurl->str,
              gwyData->playbackurl->str, gwyData->gwyip->str);
    return TRUE;
}

/**
 * @brief Initializes gateway attributes such as serial number, IP details , MAC details, URL details  etc.
 *
 * @param[in] gwyData Address of the structure holding gateway attributes.
 *
 * @return Returns TRUE indicating the call has finished.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
//Initializes gateway data item
gboolean init_gwydata(GwyDeviceData* gwydata)
{
    gwydata->serial_num = g_string_new(NULL);
    gwydata->isgateway=FALSE;
    gwydata->requirestrm=TRUE;
    gwydata->gwyip = g_string_new(NULL);
    gwydata->gwyipv6 = g_string_new(NULL);
    gwydata->hostmacaddress = g_string_new(NULL);
    gwydata->recvdevtype = g_string_new(NULL);
    gwydata->devicetype = g_string_new(NULL);
    gwydata->buildversion = g_string_new(NULL);
    gwydata->dsgtimezone = g_string_new(NULL);
    gwydata->dataGatewayIPaddress = g_string_new(NULL);
    gwydata->dstoffset = 0;
    gwydata->dstsavings = 0;
    gwydata->rawoffset = 0;
    gwydata->usesdaylighttime = FALSE;
    gwydata->baseurl = g_string_new(NULL);
    gwydata->basetrmurl = g_string_new(NULL);
    gwydata->playbackurl = g_string_new(NULL);
    gwydata->dnsconfig = g_string_new(NULL);
    gwydata->etchosts = g_string_new(NULL);
    gwydata->systemids = g_string_new(NULL);
    gwydata->receiverid = g_string_new(NULL);
    gwydata->gatewaystbip = g_string_new(NULL);
    gwydata->ipv6prefix = g_string_new(NULL);
    gwydata->devicename = g_string_new(NULL);
    gwydata->bcastmacaddress = g_string_new(NULL);
    gwydata->modelclass = g_string_new(NULL);
    gwydata->modelnumber = g_string_new(NULL);
    gwydata->deviceid = g_string_new(NULL);
    gwydata->hardwarerevision = g_string_new(NULL);
    gwydata->softwarerevision = g_string_new(NULL);
    gwydata->managementurl = g_string_new(NULL);
    gwydata->make = g_string_new(NULL);
    gwydata->accountid = g_string_new(NULL);
    gwydata->devFoundFlag=FALSE;
    gwydata->isOwnGateway=FALSE;
    gwydata->isRouteSet=FALSE;
    gwydata->sproxy=NULL;
    gwydata->sproxy_i=NULL;
    gwydata->sproxy_m=NULL;
    gwydata->sproxy_t=NULL;
    gwydata->sproxy_g=NULL;
    gwydata->sproxy_q=NULL;
    return TRUE;
}

/**
 * @brief Uninitialize gateway data attributes by resetting them to default value.
 *
 * @param[in] gwyData Address of the structure variable holding Gateway attributes.
 *
 * @return Returns a boolean value indicating success/failure of the call.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
gboolean free_gwydata(GwyDeviceData* gwydata)
{
    if(gwydata)
    {
        g_string_free(gwydata->serial_num, TRUE);
        g_string_free(gwydata->gwyip, TRUE);
        g_string_free(gwydata->gwyipv6, TRUE);
        g_string_free(gwydata->hostmacaddress, TRUE);
        g_string_free(gwydata->recvdevtype, TRUE);
        g_string_free(gwydata->devicetype, TRUE);
        g_string_free(gwydata->buildversion, TRUE);
        g_string_free(gwydata->dataGatewayIPaddress, TRUE);
        g_string_free(gwydata->dsgtimezone, TRUE);
        g_string_free(gwydata->baseurl, TRUE);
        g_string_free(gwydata->basetrmurl, TRUE);
        g_string_free(gwydata->playbackurl, TRUE);
        g_string_free(gwydata->dnsconfig, TRUE);
        g_string_free(gwydata->etchosts, TRUE);
        g_string_free(gwydata->systemids, TRUE);
        g_string_free(gwydata->receiverid, TRUE);
        g_string_free(gwydata->gatewaystbip, TRUE);
        g_string_free(gwydata->ipv6prefix, TRUE);
        g_string_free(gwydata->devicename, TRUE);
        g_string_free(gwydata->bcastmacaddress, TRUE);
        if(gwydata->sproxy)
        {
            g_clear_object(&(gwydata->sproxy));
        }
        if(gwydata->sproxy_i)
        {
            g_string_free(gwydata->make, TRUE);
            g_string_free(gwydata->accountid, TRUE);
            g_string_free(gwydata->modelnumber, TRUE);
            g_clear_object(&(gwydata->sproxy_i));
        }
        if(gwydata->sproxy_m)
        {
            g_clear_object(&(gwydata->sproxy_m));
        }
        if(gwydata->sproxy_t)
        {
            g_clear_object(&(gwydata->sproxy_t));
        }
        if(gwydata->sproxy_g)
        {
            g_clear_object(&(gwydata->sproxy_g));
        }
        if(gwydata->sproxy_q)
        {
            g_clear_object(&(gwydata->sproxy_q));
        }
    }
    return TRUE; /*CID-10127*/
}

/**
 * @brief Check for any addition or deletion of gateway from the list. If any update occurs, this routine
 * will validate and add/delete the entry to/from the gateway list and the updated list will be published
 * to others.
 *
 * @param[in] gwyData Address of the structure holding Gateway attributes.
 *
 * @return Returns TRUE if successfully updated the gateway list else returns FALSE.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
gboolean update_gwylist(GwyDeviceData* gwydata)
{
    char* sno = gwydata->serial_num->str;
    gchar* gwipaddr = gwydata->gwyip->str;
    if (sno != NULL)
    {
        GList* xdevlistitem = g_list_find_custom (xdevlist, sno, (GCompareFunc)g_list_find_sno);
        //If item already exists delete it - may be efficient than looking for
        //all the data items and updating them
        if (xdevlistitem != NULL)
        {
            gint result = g_strcmp0(g_strstrip(((GwyDeviceData *)xdevlistitem->data)->gwyip->str),g_strstrip(gwipaddr));
#ifdef ENABLE_ROUTE
            if ((disConf->enableGwSetup == TRUE) && (gwydata->isRouteSet) && (result != 0))
            {
                g_message("***** stored device gwip = %s new device gwip = %s result = %d sno = %s *****",(((GwyDeviceData *)xdevlistitem->data)->gwyip->str),g_strstrip(gwipaddr),result,sno);
                char command[128];
                //sprintf(command, "route del default gw %s", gwdata->gwyip->str);
                sprintf(command, "route del default gw %s", gwydata->gwyip->str);
                system(command);
                g_message("Clearing the default gateway %s from route list", gwydata->gwyip->str);
            }
#endif
            if (delete_gwyitem(sno) == FALSE)
            {
                //g_print("Item found, but not able to delete it from list\n");
                g_message("Found device: %s in the list, but unable to delete for update", sno);
                return FALSE;
            }
            else
            {
                //g_print("Deleted existing item\n");
                g_message("Found device: %s in the list, Deleted before update", sno);
            }
        }
        else
        {
            g_message("Item %s not found in the list", sno);
        }

        g_mutex_lock(mutex);
        xdevlist = g_list_insert_sorted_with_data(xdevlist, gwydata,(GCompareDataFunc)g_list_compare_sno, NULL);
        g_mutex_unlock(mutex);
        g_message("Inserted new/updated device %s in the list", sno);
        sendDiscoveryResult(disConf->outputJsonFile);
        if(xdevlistitem)
          xdevlistitem=NULL;
        return TRUE;
    }
    else
    {
        //g_print("Serial numer is NULL\n");
        g_critical("Got a device with empty serianl number");
        return FALSE;
    }
}

/**
 * @brief Deletes a gateway entry from the list of Gateway devices if it matches
 * with the serial number supplied as input.
 *
 * @param[in] serial_num Serial number of the gateway device represented in string format.
 *
 * @return Returns TRUE if successfully deleted an item from the Gateway device list else returns FALSE.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
gboolean delete_gwyitem(const char* serial_num)
{
    //look if the item exists
    GList* lstXdev = g_list_find_custom (xdevlist, serial_num, (GCompareFunc)g_list_find_sno);
    //if item exists delete the item
    if (lstXdev)
    {
        GwyDeviceData *gwdata = lstXdev->data;
        if(gwdata->isgateway == TRUE)
            g_message("Removing Gateway Device %s from the device list", gwdata->serial_num->str);
        else
            g_message("Removing Client Device %s from the device list", gwdata->serial_num->str);
/*        g_mutex_lock(mutex);
        free_gwydata(gwdata);
        g_free(gwdata);
        xdevlist = g_list_remove(xdevlist, gwdata);
        g_mutex_unlock(mutex);
        g_message("Deleted device %s from the list", serial_num);
        if(lstXdev)
          lstXdev=NULL;
        return TRUE;
        GwyDeviceData *gwdata = lstXdev->data;*/
        g_mutex_lock(mutex);
        xdevlist = g_list_remove_link(xdevlist, lstXdev);
        free_gwydata(gwdata);
        g_free(gwdata);
        g_mutex_unlock(mutex);
        g_message("Deleted device %s from the list", serial_num);
        if(lstXdev)
          g_list_free (lstXdev);
        return TRUE;
    }
    else
    {
        g_message("Device %s to be removed not in the discovered device list", serial_num);
    }
    return FALSE;
}

/**
 * @brief This will store the updated device discovery results in local storage as JSON formatted data.
 *
 * Whenever there's add/delete of gateway list it will create a json file, and all service variables will
 * be added to output.json. It will notify to all the listeners that listen for change of xupnp data.
 *
 * @param[in] outfilename Name of the output file to store JSON data.
 *
 * @return Returns TRUE when successfully stored the device discovery result else returns FALSE.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
gboolean sendDiscoveryResult(const char* outfilename)
{
    gboolean firstXG1GwData=TRUE;
    gboolean firstXG2GwData=TRUE;
    const gchar v4ModeValue[]="ipv4";
    const gchar v6ModeValue[]="ipv6";
    gboolean ipModeStateChanged=FALSE;

    GString *localOutputContents=g_string_new(NULL);
    g_string_printf(localOutputContents, "{\n\"xmediagateways\":\n\t[");

    GList *xdevlistDup=NULL;
    g_mutex_lock(mutex);
    if((xdevlist) && (g_list_length(xdevlist) > 0))
    {
        xdevlistDup = g_list_copy(xdevlist);
        g_mutex_unlock(mutex);
        GList *element;
        element = g_list_first(xdevlistDup);
        while(element)
        {
            g_string_append_printf(localOutputContents,"\n\t\t{\n\t\t\t");
            GwyDeviceData *gwdata = (GwyDeviceData *)element->data;
            if(gwdata->sproxy_i != NULL)
            {
                g_string_append_printf(localOutputContents,"\"sno\":\"%s\",\n", gwdata->serial_num->str);
                g_string_append_printf(localOutputContents,"\t\t\t\"isgateway\":\"%s\",\n", gwdata->isgateway?"yes":"no");
                g_string_append_printf(localOutputContents,"\t\t\t\"deviceName\":\"%s\",\n", gwdata->devicename->str);
//		g_string_append_printf(localOutputContents,"\t\t\t\"bcastMacAddress\":\"%s\",\n", gwdata->bcastmacaddress->str);
		g_string_append_printf(localOutputContents,"\t\t\t\"recvDevType\":\"%s\",\n", gwdata->recvdevtype->str);
		g_string_append_printf(localOutputContents,"\t\t\t\"DevType\":\"%s\",\n", gwdata->devicetype->str);
		g_string_append_printf(localOutputContents,"\t\t\t\"accountId\":\"%s\",\n", gwdata->accountid->str);
		g_string_append_printf(localOutputContents,"\t\t\t\"modelNumber\":\"%s\",\n", gwdata->modelnumber->str);
		g_string_append_printf(localOutputContents,"\t\t\t\"modelClass\":\"%s\",\n", gwdata->modelclass->str);
		g_string_append_printf(localOutputContents,"\t\t\t\"make\":\"%s\",\n", gwdata->make->str);
		g_string_append_printf(localOutputContents,"\t\t\t\"softwareRevision\":\"%s\",\n", gwdata->buildversion->str);
		g_string_append_printf(localOutputContents,"\t\t\t\"hardwareRevision\":\"%s\",\n", gwdata->hardwarerevision->str);
		g_string_append_printf(localOutputContents,"\t\t\t\"managementUrl\":\"%s\",\n", gwdata->managementurl->str);
                if(g_strrstr(g_strstrip(gwdata->devicetype->str),"XI") == NULL )
                {
                    g_string_append_printf(localOutputContents,"\t\t\t\"bcastMacAddress\":\"%s\",\n", gwdata->bcastmacaddress->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"gatewayip\":\"%s\",\n", gwdata->gwyip->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"gatewayipv6\":\"%s\",\n", gwdata->gwyipv6->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"hostMacAddress\":\"%s\",\n", gwdata->hostmacaddress->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"gatewayStbIP\":\"%s\",\n", gwdata->gatewaystbip->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"ipv6Prefix\":\"%s\",\n", gwdata->ipv6prefix->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"timezone\":\"%s\",\n", gwdata->dsgtimezone->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"rawoffset\":\"%d\",\n", gwdata->rawoffset);
                    g_string_append_printf(localOutputContents,"\t\t\t\"dstoffset\":\"%d\",\n", gwdata->dstoffset);
                    g_string_append_printf(localOutputContents,"\t\t\t\"dstsavings\":\"%d\",\n", gwdata->dstsavings);
                    g_string_append_printf(localOutputContents,"\t\t\t\"usesdaylighttime\":\"%s\",\n", gwdata->usesdaylighttime?"yes":"no");
                    g_string_append_printf(localOutputContents,"\t\t\t\"baseStreamingUrl\":\"%s\",\n", gwdata->baseurl->str);
                    if(g_strcmp0(gwdata->recvdevtype->str,"broadband"))
		    {
                        g_string_append_printf(localOutputContents,"\t\t\t\"requiresTRM\":\"%s\",\n", gwdata->requirestrm?"true":"false");
                        g_string_append_printf(localOutputContents,"\t\t\t\"baseTrmUrl\":\"%s\",\n", gwdata->basetrmurl->str);
                        g_string_append_printf(localOutputContents,"\t\t\t\"systemids\":\"%s\",\n", gwdata->systemids->str);
                    }
                    g_string_append_printf(localOutputContents,"\t\t\t\"playbackUrl\":\"%s\",\n", gwdata->playbackurl->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"dnsconfig\":\"%s\",\n", gwdata->dnsconfig->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"hosts\":\"%s\",\n", gwdata->etchosts->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"dataGatewayIPaddress\":\"%s\",\n", gwdata->dataGatewayIPaddress->str);
                }
                g_string_append_printf(localOutputContents,"\t\t\t\"receiverid\":\"%s\"\n\t\t}", gwdata->receiverid->str);

	    }
            else
            {
                //g_print("\"sno\":\"%s\",\n", gwdata->serial_num->str);
                g_string_append_printf(localOutputContents,"\"sno\":\"%s\",\n", gwdata->serial_num->str);
                //"isgateway":"yes",
                //char strIsGateway[5];
	            //g_print("\t\t\t\"isgateway\":\"%s\",\n", gwdata->isgateway?g_strdup("yes"):g_strdup("no"));
                g_string_append_printf(localOutputContents,"\t\t\t\"isgateway\":\"%s\",\n", gwdata->isgateway?"yes":"no");
                //"gatewayip":"10.21.32.212",
                //g_print("\t\t\t\"gatewayip\":\"%s\",\n", gwdata->gwyip->str);
                g_string_append_printf(localOutputContents,"\t\t\t\"deviceName\":\"%s\",\n", gwdata->devicename->str);
                g_string_append_printf(localOutputContents,"\t\t\t\"bcastMacAddress\":\"%s\",\n", gwdata->bcastmacaddress->str);
                g_string_append_printf(localOutputContents,"\t\t\t\"recvDevType\":\"%s\",\n", gwdata->recvdevtype->str);
                g_string_append_printf(localOutputContents,"\t\t\t\"DevType\":\"%s\",\n", gwdata->devicetype->str);
                g_string_append_printf(localOutputContents,"\t\t\t\"buildVersion\":\"%s\",\n", gwdata->buildversion->str);
                //g_string_append_printf(outputcontents,"\t\t\t\"hostMacAddress\":\"%s\",\n");
                if(g_strrstr(g_strstrip(gwdata->devicetype->str),"XI") == NULL )
                {
                    g_string_append_printf(localOutputContents,"\t\t\t\"gatewayip\":\"%s\",\n", gwdata->gwyip->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"gatewayipv6\":\"%s\",\n", gwdata->gwyipv6->str);
                    //"timezone":"EST05EDT",
                    //g_print("\t\t\t\"timezone\":\"%s\",\n", gwdata->dsgtimezone->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"hostMacAddress\":\"%s\",\n", gwdata->hostmacaddress->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"gatewayStbIP\":\"%s\",\n", gwdata->gatewaystbip->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"ipv6Prefix\":\"%s\",\n", gwdata->ipv6prefix->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"timezone\":\"%s\",\n", gwdata->dsgtimezone->str);
                    //"baseurl":"http://10.21.32.212:8080/videoStreamInit?recorderId=T0100113218",
                    //g_print("\t\t\t\"baseurl\":\"%s\",\n", gwdata->baseurl->str);

                    g_string_append_printf(localOutputContents,"\t\t\t\"rawoffset\":\"%d\",\n", gwdata->rawoffset);
                    g_string_append_printf(localOutputContents,"\t\t\t\"dstoffset\":\"%d\",\n", gwdata->dstoffset);
                    g_string_append_printf(localOutputContents,"\t\t\t\"dstsavings\":\"%d\",\n", gwdata->dstsavings);
                    g_string_append_printf(localOutputContents,"\t\t\t\"usesdaylighttime\":\"%s\",\n", gwdata->usesdaylighttime?"yes":"no");
                    g_string_append_printf(localOutputContents,"\t\t\t\"baseStreamingUrl\":\"%s\",\n", gwdata->baseurl->str);
                    //g_message("\t\t\t\"requiresTRM\":\"%s\",\n", gwdata->requirestrm?g_strdup("true"):g_strdup("false"));
                    g_string_append_printf(localOutputContents,"\t\t\t\"requiresTRM\":\"%s\",\n", gwdata->requirestrm?"true":"false");
                    g_string_append_printf(localOutputContents,"\t\t\t\"baseTrmUrl\":\"%s\",\n", gwdata->basetrmurl->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"playbackUrl\":\"%s\",\n", gwdata->playbackurl->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"dnsconfig\":\"%s\",\n", gwdata->dnsconfig->str);
                    //"hosts":"127.0.0.1 localhost  # for using the ssh tunnel;127.0.0.1 intel_ce_linux;",
                    //g_print("\t\t\t\"hosts\":\"%s\",\n", gwdata->etchosts->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"hosts\":\"%s\",\n", gwdata->etchosts->str);
                    //"systemids":"channelMapId=1901;controllerId=3414;plantId=3898094610;vodServerId=63301",
                    //g_print("\t\t\t\"systemids\":\"%s\",\n", gwdata->systemids->str);
                    g_string_append_printf(localOutputContents,"\t\t\t\"systemids\":\"%s\",\n", gwdata->systemids->str);
                    //"receiverid":"T0100113218"
                    //g_print("\t\t\t\"receiverid\":\"%s\"\n\t\t}", gwdata->receiverid->str);
                }
                else
                {
                    g_string_append_printf(localOutputContents,"\t\t\t\"dataGatewayIPaddress\":\"%s\",\n", gwdata->dataGatewayIPaddress->str);
                }

                //"dnsconfig":"search plant1.dac10.he.cc.ccp.cable.comcast.com;nameserver 10.252.180.16;",
                //g_print("\t\t\t\"dnsconfig\":\"%s\",\n", gwdata->dnsconfig->str);
                g_string_append_printf(localOutputContents,"\t\t\t\"receiverid\":\"%s\"\n\t\t}", gwdata->receiverid->str);
            }
            if((disConf->enableGwSetup == TRUE) && ((firstXG1GwData == TRUE) || (firstXG2GwData == TRUE)) && (gwdata->isgateway == TRUE) && (checkvalidhostname(gwdata->dnsconfig->str) == TRUE ) && (checkvalidhostname(gwdata->etchosts->str) == TRUE) && (checkvalidip(gwdata->gwyip->str) == TRUE) && (checkvalidip(gwdata->gwyipv6->str) == TRUE))
            {

#ifdef ENABLE_ROUTE
                GString *GwRouteParam=g_string_new(NULL);
                g_string_printf(GwRouteParam,"%s" ,disConf->gwSetupFile);
                g_string_append_printf(GwRouteParam," \"%s\"" ,gwdata->gwyip->str);
                g_string_append_printf(GwRouteParam," \"%s\"" ,gwdata->dnsconfig->str);
                g_string_append_printf(GwRouteParam," \"%s\"" ,gwdata->etchosts->str);
                g_string_append_printf(GwRouteParam," \"%s\"" ,disConf->gwIf);
                g_string_append_printf(GwRouteParam," \"%d\"" ,disConf->GwPriority);
                g_string_append_printf(GwRouteParam," \"%s\"" ,gwdata->ipv6prefix->str);
                g_string_append_printf(GwRouteParam," \"%s\"" ,gwdata->gwyipv6->str);
                g_string_append_printf(GwRouteParam," \"%s\"" ,gwdata->devicetype->str);
//            	g_string_append_printf(GwRouteParam," %s " ,"&");
                g_message("Calling gateway script %s",GwRouteParam->str);
                system(GwRouteParam->str);
                g_string_free(GwRouteParam,TRUE);
                gwdata->isRouteSet = TRUE;
                if(firstXG1GwData == TRUE)
                {
                    firstXG1GwData=FALSE;
                }
                else if(firstXG2GwData == TRUE)
                {

                    firstXG2GwData=FALSE;
                }
#endif
                if (g_strrstr(g_strstrip(gwdata->ipv6prefix->str),"null") ||  ! *(gwdata->ipv6prefix->str))
                {

                    if( g_strcmp0(ipMode->str,v4ModeValue) != 0 )
                    {
                        g_message("v4 mode changed");
                        ipModeStateChanged=TRUE;
                        g_string_assign(ipMode,v4ModeValue);
                    }
                }
                else
                {
                    if( g_strcmp0(ipMode->str,v6ModeValue) != 0 )
                    {
                        g_message("v6 mode changed");
                        ipModeStateChanged=TRUE;
                        g_string_assign(ipMode,v6ModeValue);
                    }
                }

                if (ipModeStateChanged == TRUE)
                {
#if defined(USE_XUPNP_IARM) || defined(USE_XUPNP_IARM_BUS)
                    broadcastIPModeChange();
#endif
                }

#ifdef USE_XUPNP_TZ_UPDATE
                if (g_strcmp0(g_strstrip(gwdata->dsgtimezone->str),"null") != 0)
                    broadcastTimeZoneChange(gwdata);
#endif

            }
            element = g_list_next(element);
            if (element) g_string_append_printf(localOutputContents, ",");
        }
	if(element)
          g_list_free(element);
    if(xdevlistDup)
        g_list_free(xdevlistDup);
    }
    else
    {
        g_message("Send Discovery Result : Device List Null or Empty");
        g_mutex_unlock(mutex);
    }

    //g_print("\n\t]\n}\n");
    g_string_append_printf(localOutputContents,"\n\t]\n}\n");
    //g_print("\nOutput is\n%s", outputcontents->str);
    g_message("Output is\n%s", localOutputContents->str);
    g_mutex_lock(devMutex);
    g_string_assign(outputcontents,localOutputContents->str);
    g_mutex_unlock(devMutex);

    //IARM Update
#if defined(USE_XUPNP_IARM)
    notifyXUPnPDataUpdateEvent(UIDEV_EVENT_XUPNP_DATA_UPDATE);
#elif defined(USE_XUPNP_IARM_BUS)
    notifyXUPnPDataUpdateEvent(IARM_BUS_SYSMGR_EVENT_XUPNP_DATA_UPDATE);
#endif
    //End - IARM Update

    if (g_file_set_contents(outfilename, localOutputContents->str, -1, NULL)==FALSE)
    {
        g_string_free(localOutputContents,TRUE);
        g_print("xdiscovery:Problem in updating the output file contents\n");
        g_critical("Problem in updating the file contents");
        return FALSE;
    } else {
        g_string_free(localOutputContents,TRUE);
    }

    return TRUE;
}

/*Thread to continuously pole and verify that existing devices are alive and responding
 If not take them off from device list*/

//#ifdef GUPNP_0_19

/**
 * @brief Verify and update the gateway list according to the availability of the gateway devices
 * connected to the network. New devices will be added to the list and dead devices will be deleted
 * from the list.
 *
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
void* verify_devices()
{
    guint lenPrevDevList = 0;
    guint lenCurDevList = 0;
    guint lenXdevList = 0;
    guint checkMainLoopCounter=0;
//workaround to remove device in second attempt -Start
    guint removeDeviceNo=0;
    guint counter=0;
    guint preCounter=0;
    guint removeDeviceNo1=0;
    guint counter1=0;
    guint preCounter1=0;
    guint sleepCounter=0;
//workaround to remove device in second attempt -Start
    while(1)
    {
        //g_main_context_push_thread_default(main_context);
        if (! g_main_loop_is_running(main_loop))
        {
          if(checkMainLoopCounter < 7)
            g_message("TELEMETRY_XUPNP_DISCOVERY_MAIN_LOOP_NOT_RUNNING");
          checkMainLoopCounter++;
        }
        if(deviceAddNo)
        {
            if((sleepCounter > 6) && (sleepCounter < 12)) //wait for device addition to complete in 60 seconds and print only for another 60 seconds if there is a hang
            {
                g_message("Device Addition %u going in main loop",deviceAddNo);
            }
            sleepCounter++;
            usleep(XUPNP_RESCAN_INTERVAL);
            continue;
        }
        sleepCounter=0;
	if(rfc_enabled)
	{
	    if (gssdp_resource_browser_rescan(GSSDP_RESOURCE_BROWSER(cp_client))==FALSE)
	    {
		g_message("Forced rescan failed for renderer");
		usleep(XUPNP_RESCAN_INTERVAL);
	    }
	    if (gssdp_resource_browser_rescan(GSSDP_RESOURCE_BROWSER(cp_gw))==FALSE)
	    {
		g_message("Forced rescan failed for gateway");
		usleep(XUPNP_RESCAN_INTERVAL);
	    }
	    if (gssdp_resource_browser_rescan(GSSDP_RESOURCE_BROWSER(cp_bgw))==FALSE)
	    {
		g_message("Forced rescan failed for broadband");
		usleep(XUPNP_RESCAN_INTERVAL);
	    }
	}
        if (gssdp_resource_browser_rescan(GSSDP_RESOURCE_BROWSER(cp))==FALSE)
        {
            g_message("Forced rescan failed\n");
            //g_debug("Forced rescan failed, sleeping");
            usleep(XUPNP_RESCAN_INTERVAL);
            continue;
        }
        //else
        //    g_print("Forced rescan success\n");
        //g_debug("Forced rescan success");
        usleep(XUPNP_RESCAN_INTERVAL);
/*        const GList *constLstProxies = gupnp_control_point_list_device_proxies(cp);
        counter++;
        counter1++;
        if (NULL == constLstProxies)
        {
            if(disConf->enableGwSetup == TRUE)
            {
                g_message("Error in getting list of device proxies or No device proxies found");
            }
            if( removeDeviceNo == 0 )
            {
                removeDeviceNo++;
                preCounter=counter;
            }
            else if(( removeDeviceNo == 1) && (preCounter == (counter-1)))
            {
                removeDeviceNo=0;
                delOldItemsFromList(TRUE);
                counter=0;
                preCounter=0;
            }
            else
            {
                removeDeviceNo=0;
                counter=0;
                preCounter=0;

            }
            continue;
        }
        GList *xdevlistDup=NULL;

        lenCurDevList = g_list_length(constLstProxies);
        g_mutex_lock(mutex);
        if((xdevlist) && (g_list_length(xdevlist) > 0))
        {
            xdevlistDup = g_list_copy(xdevlist);
            lenXdevList = g_list_length(xdevlistDup);
        }
        g_mutex_unlock(mutex);


//        lenXdevList = g_list_length(xdevlist);
        //g_message("Current list length is %u\n",lenCurDevList);

        if (lenCurDevList > 0)
        {
            //Compare new list with existing list
            if (lenXdevList > 0)
            {
                gboolean existsFlag = FALSE;
                gboolean delCheckFlag = FALSE;
                GList *element = NULL;

                element = g_list_first(xdevlistDup);
                while(element)
                {
                    GwyDeviceData *gwdata = (GwyDeviceData *)element->data;

                    //Loop through all new found devices
                    GList* lstProxies = g_list_first(constLstProxies);
                    //g_message("Length of lstProxies is %u", g_list_length(lstProxies));
                    existsFlag = FALSE;
                    while(lstProxies)
                    {
                        if (NULL == lstProxies->data)
                        {
                            g_message("1-Error - Got Null pointer as deviceproxy");
                            continue;
                        }
                        const GUPnPDeviceProxy* dproxy;
                        dproxy = (GUPnPDeviceProxy *)lstProxies->data;
                        gchar* sno = gupnp_device_info_get_serial_number (GUPNP_DEVICE_INFO(dproxy));
                        gint result = g_strcmp0(g_strstrip(gwdata->serial_num->str),g_strstrip(sno));
                        g_free(sno);
                        lstProxies = g_list_next(lstProxies);
                        //Matched the serial number - No need to do anything - quit the loop
                        if (result==0)
                        {
                            existsFlag=TRUE;
                            break;
                        }

                        //There are still elements to search

                    }
                    //g_list_free(lstProxies);
                    //End - Loop through all new found devices

                    //Device not found in new device list. Mark it for deletion later.
                    if (existsFlag==FALSE)
                    {
                        delCheckFlag = TRUE;
                        gwdata->devFoundFlag = FALSE;
                    }

                    //There are still elements to search
                    element = g_list_next(element);
                }
                if (delCheckFlag == TRUE)
                {
                    //	g_message("delCheckFlag is enabled ");
                    if( removeDeviceNo1 == 0 )
                    {
                        removeDeviceNo1++;
                        preCounter1=counter1;
                    }
                    else if(( removeDeviceNo1 == 1) && (preCounter1 == (counter1-1)))
                    {
                        removeDeviceNo1=0;
                        delOldItemsFromList(FALSE);
                        counter1=0;
                        preCounter1=0;
                    }
                    else
                    {
                        removeDeviceNo1=0;
                        counter1=0;
                        preCounter1=0;
                    }
                }
            }
        }
        else
        {
            //g_message("Device List length - Current length: %u - Previous Length: %u",lenCurDevList, lenPrevDevList);
            g_message("Current dev length is empty ");
            delOldItemsFromList(TRUE);
        }
	if(xdevlistDup)
	{
	    g_list_free(xdevlistDup);
	}
/*
                  //Find out newly discovered devices and add to cleaned up list
                lenXdevList = g_list_length(xdevlist);
                if (lenCurDevList > 0)
                {
                      //Loop through all new found devices
                      //g_message("Device List length change - Current length: %u - Previous Length: %u - xdevlist len %u",lenCurDevList, lenPrevDevList, lenXdevList);


                      gboolean existsFlag = FALSE;
                      GList* lstProxies = g_list_first(constLstProxies);
                      while(lstProxies)
                      {
                          if (NULL == lstProxies->data)
                          {
                            g_message("2-Error - Got Null pointer as deviceproxy");
                            continue;
                          }
                          const GUPnPDeviceProxy* dproxy;
        		  dproxy = (GUPnPDeviceProxy *)lstProxies->data;

                          //g_print("Serial Number %s\n", sno, lenCurDevList);
                          existsFlag = FALSE;
                          if (lenXdevList > 0)
                          {

                            gchar* sno = gupnp_device_info_get_serial_number (GUPNP_DEVICE_INFO (dproxy));
                              //g_message("Searching for serial number %s", sno);
                            existsFlag = checkDeviceExists(sno);
                          }
                          //End - Loop through all new found devices

                          //Device not found in device list. Mark it for addition.
                          if (existsFlag==FALSE)
                          {
                              device_proxy_available_cb(cp, (GUPnPDeviceProxy *)lstProxies->data);
                          }

                          //There are still elements to search
                          lstProxies = g_list_next(lstProxies);
                      }
                      //g_list_free(lstProxies);

                      //g_print("Current list is greater\n");
                  }

*/


        //After cleaning up, update the prev list length
//        lenPrevDevList=lenCurDevList;
        //g_print("\nWaiting Discovery...\n");
    }
}

/**
 * @brief Delete all the devices from the device list or delete devices marked
 * as "Not Found" from device list.
 *
 * @param[in] bDeleteAll Boolean variable indicating whether to delete all entries.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
void delOldItemsFromList(gboolean bDeleteAll)
{
    guint lenXDevList = 0;
    guint deletedDeviceNo=0;
    lenXDevList = g_list_length(xdevlist);
    //g_message("Length of the list before deletion is %u", lenXDevList);
    if (lenXDevList > 0)
    {
        GwyDeviceData *gwdata = NULL;
        GList *element = g_list_first(xdevlist);

        while (element && (lenXDevList > 0))
        {
            gwdata = element->data;
            element = g_list_next(element);
            if (gwdata->isOwnGateway==FALSE)
            {
                if (gwdata->devFoundFlag==FALSE || bDeleteAll==TRUE)
                {
                    if(gwdata->isgateway == TRUE)
                        g_message("Removing Gateway Device %s from the device list", gwdata->serial_num->str);
                    else
                        g_message("Removing Client Device %s from the device list", gwdata->serial_num->str);
#ifdef ENABLE_ROUTE
                    if ((disConf->enableGwSetup == TRUE) && (gwdata->isRouteSet) && checkvalidip(gwdata->gwyip->str))
                    {
                        char command[128];
                        sprintf(command, "route del default gw %s", gwdata->gwyip->str);
                        system(command);
                        g_message("Clearing the default gateway %s from route list", gwdata->gwyip->str);
                    }
#endif
                    g_mutex_lock(mutex);
                    deletedDeviceNo++;
                    free_gwydata(gwdata);
                    g_free(gwdata);
                    xdevlist = g_list_remove(xdevlist, gwdata);
                    //g_print("Element Removed\n");

                    g_mutex_unlock(mutex);
                }
            }
            lenXDevList--;
        }
        g_list_free(element);
        if(deletedDeviceNo > 0)
        {
            g_message("removed %d devices,sending updated discovery result",deletedDeviceNo);
            sendDiscoveryResult(disConf->outputJsonFile);
        }
    }
}

static void on_last_change (GUPnPServiceProxy *sproxy, const char  *variable_name, GValue  *value, gpointer user_data)
{
    GwyDeviceData *gwdata = NULL;
    const char* updated_value;
    int updated_value_int;
    gboolean bUpdateDiscoveryResult=FALSE;
    const char* udn = gupnp_service_info_get_udn(GUPNP_SERVICE_INFO(sproxy));
    g_message("Variable name is %s udn is %s", variable_name, udn);

    if (udn != NULL)
    {
        if (g_strrstr(udn,"uuid:"))
        {
            gchar* receiverid = g_strndup(udn+5, strlen(udn)-5);
            GList* xdevlistitem = g_list_find_custom (xdevlist, receiverid, (GCompareFunc)g_list_find_udn);
            if (xdevlistitem != NULL)
            {
                gwdata = xdevlistitem->data;
                g_message("Found device: %s - Receiver id %s in the list, Updating %s",
                          gwdata->serial_num->str,gwdata->receiverid->str, variable_name);
                if (g_strcmp0(g_strstrip(variable_name),"PlaybackUrl") == 0)
                {
                    updated_value = g_value_get_string(value);
                    g_message("Updated value is %s ", updated_value);
                    if(g_strcmp0(g_strstrip(updated_value),gwdata->playbackurl->str) != 0)
                    {
                        bUpdateDiscoveryResult=TRUE;
                        g_string_assign(gwdata->playbackurl, updated_value);

                        //Check if it is local device and replace MoCA IP with 127.0.0.1
                        //if ((gwdata->isgateway == TRUE))
                        //{
                        if (replace_local_device_ip(gwdata) == TRUE)
                        {
                            g_message("Replaced MocaIP for %s with localhost", gwdata->serial_num->str);
                        }
                        //}
                    }

                }

                if (g_strcmp0(g_strstrip(variable_name),"SystemIds") == 0)
                {
                    updated_value = g_value_get_string(value);
                    g_message("Updated value is %s ", updated_value);
                    if(g_strcmp0(g_strstrip(updated_value),gwdata->systemids->str) != 0)
                    {
                        bUpdateDiscoveryResult=TRUE;
                        g_string_assign(gwdata->systemids, updated_value);
                    }
                }
                if (g_strcmp0(g_strstrip(variable_name),"DataGatewayIPaddress") == 0)
                {
                    updated_value = g_value_get_string(value);
                    g_message("Updated value of DataGatewayIPaddressis %s ", updated_value);
                    if(g_strcmp0(g_strstrip(updated_value),gwdata->dataGatewayIPaddress->str) != 0)
                    {
                        bUpdateDiscoveryResult=TRUE;
                        g_string_assign(gwdata->dataGatewayIPaddress, updated_value);
                    }
                }
                if (g_strcmp0(g_strstrip(variable_name),"DnsConfig") == 0)
                {
                    updated_value = g_value_get_string(value);
                    g_message("Updated value is %s ", updated_value);
                    if(g_strcmp0(g_strstrip(updated_value),gwdata->dnsconfig->str) != 0)
                    {
                        bUpdateDiscoveryResult=TRUE;
                        g_string_assign(gwdata->dnsconfig, updated_value);
                    }
                }
                if (g_strcmp0(g_strstrip(variable_name),"TimeZone") == 0)
                {
                    updated_value = g_value_get_string(value);
                    g_message("Updated value is %s ", updated_value);
                    if(g_strcmp0(g_strstrip(updated_value),gwdata->dsgtimezone->str) != 0)
                    {
                        bUpdateDiscoveryResult=TRUE;
                        g_string_assign(gwdata->dsgtimezone, updated_value);
#ifdef USE_XUPNP_TZ_UPDATE
                    if (g_strcmp0(g_strstrip(gwdata->dsgtimezone->str),"null") != 0)
                        broadcastTimeZoneChange(gwdata);
#endif
                    }
                }
                //update_gwylist(gwdata);
                g_free(receiverid);
                if(bUpdateDiscoveryResult)
                    sendDiscoveryResult(disConf->outputJsonFile);

            }
        }
    }
    return;
}
/**
 * @brief This will parse and load the configuration file in a key/value pair format as specified
 * in GLib encoding. The configuration file contain attributes which are categorized into following:
 * - Network   : Contains parameters such as interface name, IP etc.
 * - Datafiles : Contains Name of Script and Log files.
 *
 * @param[in] configfile Name of the configuration file.
 *
 * @return Returns TRUE if successfully loaded the configuration file in GKey file structure else returns FALSE.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
gboolean readconffile(const char* configfile)
{

    GKeyFile *keyfile = NULL;
    GKeyFileFlags flags;
    GError *error = NULL;
    gsize length;

    if(configfile)
    {

        /* Create a new GKeyFile object and a bitwise list of flags. */
        keyfile = g_key_file_new ();
        flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

        /* Load the GKeyFile from keyfile.conf or return. */
        if (!g_key_file_load_from_file (keyfile, configfile, flags, &error))
        {
            g_error(error->message);
            g_clear_error(&error);
            g_key_file_free(keyfile);
            return FALSE;
        }
        g_message("Starting with Settings %s\n", g_key_file_to_data(keyfile, NULL, NULL));

        disConf = g_new(ConfSettings,1);
        /*
        # Names of all network interfaces used for publishing
        [Network]
        discIf=eth1
        GwIf=eth1
        GwPriority=50
        */

        /* Read in data from the key file from the group "Network". */
        disConf->discIf = g_key_file_get_string(keyfile, "Network","discIf", NULL);
        disConf->gwIf = g_key_file_get_string(keyfile, "Network","GwIf", NULL);
        disConf->GwPriority = g_key_file_get_integer(keyfile, "Network","GwPriority", NULL);
        /*
        /*
        # Paths and names of all input data files
        [DataFiles]
        gwSetupFile=/lib/rdk/gwSetup.sh
        LogFile=/opt/logs/xdiscovery.log
        outputJsonFile=/opt/output.json
        */
        /* Read in data from the key file from the group "DataFiles". */
        disConf->gwSetupFile = g_key_file_get_string(keyfile, "DataFiles","gwSetupFile", NULL);
        disConf->logFile = g_key_file_get_string(keyfile, "DataFiles","LogFile", NULL);
        disConf->outputJsonFile = g_key_file_get_string(keyfile, "DataFiles","outputJsonFile", NULL);
        /*
        # Enable/Disable feature flags
        enableGwSetup=TRUE
         */
        disConf->enableGwSetup = g_key_file_get_boolean(keyfile, "Flags","enableGwSetup", NULL);

        g_key_file_free(keyfile);
        if (disConf->logFile == NULL)
            g_warning("configuration doesnt have log file so log file should be given explicitly or it will be written in console\n");

        if (disConf->discIf == NULL)
        {
            g_warning("Invalid or no values found for mandatory parameters\n");
            return FALSE;
        }

        return TRUE;
    }
    else
    {
        g_warning("No configuration file \n");
        return FALSE;
    }
}

/**
 * @brief Get the serial number of the device from Vendor specific UDHCPC config file.
 *
 * @param[out] ownSerialNo Address of the string to return the Serial number.
 *
 * @return Returns TRUE if successfully get the own serial number else returns FALSE.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
gboolean getserialnum(GString* ownSerialNo)
{
    GError                  *err=NULL;
    gboolean                result;
    gchar* udhcpcvendorfile = NULL;
    gchar *tokens = NULL;
    result = g_file_get_contents ("//etc//udhcpc.vendor_specific", &udhcpcvendorfile, NULL, &err);
    if (result)
    {
        tokens = g_strsplit_set(udhcpcvendorfile," \n\t\b\0", -1);
        guint toklength = g_strv_length(tokens);
        guint loopvar=0;
        for (loopvar=0; loopvar<toklength; loopvar++)
        {
            //g_print("Token is %s\n",g_strstrip(tokens[loopvar]));
            if (g_strrstr(g_strstrip(tokens[loopvar]), "SUBOPTION4"))
            {
                if ((loopvar+1) < toklength )
                    g_string_assign(ownSerialNo,g_strstrip(tokens[loopvar+2]));
                g_message("getserialnum serial no is %s",ownSerialNo->str);
                result = TRUE;
            }
        }
        if (tokens) g_strfreev(tokens);
    }
    if (err) g_clear_error(&err);
    if (udhcpcvendorfile) g_free(udhcpcvendorfile);
    return result;
}

/**
 * @brief Replace all occurrences of a sub-string in the given string to value the specified.
 *
 * @param[in] src_string Source String.
 * @param[in] sub_string Sub string which has to be searched and replaced.
 * @param[in] replace_string New sub string that will be replaced.
 *
 * @return Returns TRUE if successfully replaced the sub string with replace string else returns FALSE.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
char *replace_string(char *src_string, char *sub_string, char *replace_string) {

    char *return_str, *insert_str, *tmp_str;
    int distance, lenSubStr, lenRepStr, numOccurences;

    if ((!src_string) || (!sub_string) || (!replace_string))return src_string;

    lenSubStr = strlen(sub_string);
    lenRepStr = strlen(replace_string);

    insert_str = src_string;
    for (numOccurences = 0; tmp_str = strstr(insert_str, sub_string); ++numOccurences) {
        insert_str = tmp_str + lenSubStr;
    }

    tmp_str = return_str = malloc(strlen(src_string) + (lenRepStr - lenSubStr) * numOccurences + 1);

    if (!return_str) return src_string;

    while (numOccurences--) {
        insert_str = strstr(src_string, sub_string);
        distance = insert_str - src_string;
        tmp_str = strncpy(tmp_str, src_string, distance) + distance;
        tmp_str = strcpy(tmp_str, replace_string) + lenRepStr;
        src_string += distance + lenSubStr;
    }
    strcpy(tmp_str, src_string);
    return return_str;
}

/**
 * @brief This function gets the IP address for the given interface. The IP Address will be
 * formatted to IPv6 or IPv4 address depending upon the configuration.
 *
 * @param[in] ifname Name of the network interface.
 * @param[out] ipAddressBuffer String which will store the result IP Address.
 * @param[in] ipv6Enabled Boolean value indicating whether IPv6 is enabled.
 *
 * @return Returns an Integer value.
 * @retval 1 If an IP address is found.
 * @retval 0 If No IP address found on specified interface.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
int getipaddress(const char* ifname, char* ipAddressBuffer, gboolean ipv6Enabled)
{
    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;
    getifaddrs(&ifAddrStruct);
    //char addressBuffer[INET_ADDRSTRLEN] = NULL;
    int found=0;

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if ((ifa ->ifa_addr->sa_family==AF_INET6) && (ipv6Enabled == TRUE)) { // check it is IP6
            // is a valid IP6 Address
            tmpAddrPtr=&((struct sockaddr_in6  *)ifa->ifa_addr)->sin6_addr;

            inet_ntop(AF_INET6, tmpAddrPtr, ipAddressBuffer, INET6_ADDRSTRLEN);

            //if (strcmp(ifa->ifa_name,"eth0")==0)
            if (strcmp(ifa->ifa_name,ifname)==0)
            {
                found = 1;
                break;
            }
        }

        else {

            if (ifa ->ifa_addr->sa_family==AF_INET) { // check it is IP4
                // is a valid IP4 Address
                tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;

                inet_ntop(AF_INET, tmpAddrPtr, ipAddressBuffer, INET_ADDRSTRLEN);

                //if (strcmp(ifa->ifa_name,"eth0")==0)
                if (strcmp(ifa->ifa_name,ifname)==0)
                {
                    found = 1;
                    break;
                }
            }
        }
    }

    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
    return found;

}

/**
 * @brief When the discovered device is the own device then MOcA IP is replaced.
 *
 * @param[in] gwyData Address of structure holding the Gateway device attributes.
 *
 * @return Returns TRUE if successfully replaced the IP else returns FALSE.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
gboolean replace_local_device_ip(GwyDeviceData* gwydata)
{
    //if(gwydata->isgateway == TRUE)
    //{
    if ((g_strcmp0 (g_strstrip(gwydata->gwyip->str), g_strstrip(ipaddress)) == 0))
    {
        g_message("isown gateway is true");
        gwydata->isOwnGateway=TRUE;
        gboolean result = replace_hn_with_local(gwydata);
        return result;
    }
    //}
    return FALSE;
}

/**
 * @brief This will check whether the input IP Address is a valid IPv4 or IPv6 address.
 *
 * @param[in] ipAddress IP Address in string format.
 *
 * @return Returns TRUE if IP address is valid else returns FALSE.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
gboolean checkvalidip( char* ipAddress)
{
    struct in_addr addr4;
    struct in6_addr addr6;
    int validip4 = inet_pton(AF_INET, ipAddress, &addr4);
    int validip6 = inet_pton(AF_INET6, ipAddress, &addr6);
    if (g_strrstr(g_strstrip(ipAddress),"null") || ! *ipAddress )
    {
        g_message ("ipaddress are empty %s",ipAddress);
        return TRUE;
    }

    if ((validip4 == 1 ) || (validip6 == 1 ))
    {
        return TRUE;
    }
    else
    {
        g_message("Not a valid ip address %s " , ipAddress);
        return FALSE;
    }
}

/**
 * @brief This function will check whether a Host name is valid by validating all the associated IP addresses.
 *
 * @param[in] hostname Host name represented as a string.
 *
 * @return Returns TRUE if host name is valid else returns FALSE.
 * @ingroup XUPNP_XDISCOVERY_FUNC
 */
gboolean checkvalidhostname( char* hostname)
{
    if (g_strrstr(g_strstrip(hostname),"null") || ! *hostname )
    {
        g_message ("hostname  values are empty %s",hostname);
        return TRUE;
    }
    gchar **tokens = g_strsplit_set(hostname," ;\n\0", -1);
    guint toklength = g_strv_length(tokens);
    guint loopvar=0;

    for (loopvar=0; loopvar<toklength; loopvar++)
    {
        if (checkvalidip(tokens[loopvar]))
        {
	    g_strfreev(tokens);
            return TRUE;
        }
    }
    g_message(" no valid ip so rejecting the dns values %s ",hostname);
    g_strfreev(tokens); /*CID-24000*/
    return FALSE;
}

#if defined(USE_XUPNP_IARM) || defined(USE_XUPNP_IARM_BUS)
void broadcastIPModeChange(void)
{
    IARM_Bus_SYSMgr_EventData_t eventData;
    eventData.data.systemStates.stateId = IARM_BUS_SYSMGR_SYSSTATE_IP_MODE;
    eventData.data.systemStates.state = 1;
    eventData.data.systemStates.error = 0;
    strncpy(eventData.data.systemStates.payload,ipMode->str,strlen(ipMode->str));
    eventData.data.systemStates.payload[strlen(ipMode->str)]='\0';
    IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME, (IARM_EventId_t) IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE, (void *)&eventData, sizeof(eventData));
    g_message("sent event for ipmode= %s",ipMode->str);
}
#endif

#ifdef USE_XUPNP_TZ_UPDATE
void broadcastTimeZoneChange(GwyDeviceData *gwdata)
{
    IARM_Bus_SYSMgr_EventData_t eventData;
    static int prevTzIndex = -1;
    int currTZIndex = -1;
    static bool IsDst = false;

    //g_message("broadcastTimeZoneChange: Input  Payload - %s",gwdata->dsgtimezone->str);
    /* get the Mapped Time Zone for Sys Mgr */
    int size = sizeof(m_timezoneinfo)/sizeof(struct _Timezone);
    int i = 0;
    for (i = 0; i < size; i++ )
    {
        if (g_strcmp0(m_timezoneinfo[i].upnpTZ, gwdata->dsgtimezone->str) == 0)
        {
            currTZIndex = i;
            break;
        }
    }

    /* No Valid Mapped Time Zone Found , return */
    if(currTZIndex == -1)
    {
        g_message("broadcastTimeZoneChange: Do Not Update Inavlid Time Zone - %s ",gwdata->dsgtimezone->str);
        return;
    }


    //g_message("broadcastTimeZoneChange Size = %d : currTZIndex =  %d",size,currTZIndex);
    /* Broadcast only on Change of Time Zone Information */
    if((prevTzIndex != currTZIndex) || (IsDst != gwdata->usesdaylighttime))
    {
        eventData.data.systemStates.stateId = IARM_BUS_SYSMGR_SYSSTATE_TIME_ZONE;
        eventData.data.systemStates.state = 2;
        eventData.data.systemStates.error = 0;

        /* Send the pay load based on DST */
        if(gwdata->usesdaylighttime)
        {
            snprintf(eventData.data.systemStates.payload,
                     sizeof(eventData.data.systemStates.payload),"%s", m_timezoneinfo[currTZIndex].tzInDst);
        }
        else
        {
            snprintf(eventData.data.systemStates.payload,
                     sizeof(eventData.data.systemStates.payload),"%s", m_timezoneinfo[currTZIndex].tz);
        }
        g_message("broadcastTimeZoneChange: sending SysMgr TZ Event with Payload - %s",eventData.data.systemStates.payload);

        IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME, (IARM_EventId_t) IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE, (void *)&eventData, sizeof(eventData));
        prevTzIndex = currTZIndex;
        IsDst = gwdata->usesdaylighttime;
    }
}
#endif

int getSoupStatusFromUrl(char* url)
{
    int ret=0;
    SoupSession *session = soup_session_sync_new_with_options (SOUP_SESSION_TIMEOUT,1, NULL); //timeout 1 secs
    if(session)
    {
        SoupMessage *msg = soup_message_new ("HEAD", url);
        if(msg != NULL)
        {
            soup_session_send_message (session, msg);
            if (SOUP_STATUS_IS_SUCCESSFUL(msg->status_code))
            {
                g_message("soup status for %s success\n",url);
                ret=1;
            }
        }
        else
            g_message("soup message creation failed url %s \n",url);
        soup_session_abort (session);
        g_object_unref (session);
    }
    else
        g_message("soup session creation failed\n");
    return ret;
}
