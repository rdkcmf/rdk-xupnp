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
#ifndef LIBXDISC_H
#include "libxdisc.h"
#endif

//#include <stdio.h>
#include <string.h>
#include <sys/types.h>
//#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <glib.h>
#include <libgupnp/gupnp-control-point.h>

int getipaddress(const char* ifname, char* ipAddressBuffer);
void* discover_devices(gpointer data);
static void device_reply (GUPnPServiceProxy *proxy, GUPnPServiceProxyAction *action, gpointer user_data);
static gint g_list_str_contains(gconstpointer* baseUrl, gconstpointer* udn );


//Global Data
static GMutex *mutex = NULL;
static guint discThreadFlag = 1;
static GList* xdevlist= NULL;

static GMainContext *main_context = NULL;
//static GMainLoop *main_loop = NULL;
static GUPnPContext *context = NULL;


//End - Global Data

//Data Structures for
typedef struct _discoverThreadData {
	char *upnpIfName;
	int upnpPort;
	void (*OnResultsUpdated)(char*, int length);
	void (*OnStart)(int);
	void (*OnError)(int);
} DiscoverThreadData;

typedef struct _proxyMapping {
  GUPnPServiceProxy *proxy;
  GUPnPServiceProxyAction *action;
  GSource *timeout_src;
} ProxyMapping;


//API functions
void startUpnpDiscovery(const char* upnpIfName, const unsigned int upnpPort, void (*OnStart)(int statusCode) ,void(*OnError)(int ErrorCode), void (*OnResultsUpdated)(char* upnpResults, int length) )
{
	//g_thread_init (NULL);
	g_type_init();
    
	GThread *thread; 
        GError *error = NULL;

	

	mutex = g_mutex_new ();

	//char ipAddressBuffer[INET_ADDRSTRLEN];

	DiscoverThreadData *discThrData = g_new(DiscoverThreadData, 1);
	discThrData->upnpIfName = g_new(char,strlen(upnpIfName)+1);
	g_stpcpy(discThrData->upnpIfName, upnpIfName);
	discThrData->upnpPort = upnpPort;
	discThrData->OnStart = OnStart;
	discThrData->OnError = OnError;
	discThrData->OnResultsUpdated = OnResultsUpdated;

    main_context = g_main_context_default();
    context = gupnp_context_new (main_context, discThrData->upnpIfName, discThrData->upnpPort, &error);

    if (error) {
        g_printerr ("Error creating the GUPnP context: %s\n", error->message);
        discThrData->OnError(1);
        g_error_free (error);
        return;
    }

    gupnp_context_set_subscription_timeout(context, 5);

	//main_loop = g_main_loop_new (main_context, FALSE);
    thread = g_thread_create(discover_devices, discThrData,FALSE, NULL);
	//g_main_loop_run (main_loop);

	/*Test Code
    OnResultsUpdated(upnpResults, resultsLength);
	g_print("Calling Status code\n");
	OnStart(1);
	Test Code */



}

void stopUpnpDiscovery(void (*OnStop)(void))
{
	g_print("Calling OnStop\n");

	g_mutex_lock(mutex);
	discThreadFlag = 0;
	g_mutex_unlock(mutex);

	usleep(5000000);

//	g_main_loop_unref (main_loop);
	g_object_unref (context);
	OnStop();
}
//Call back functions

static void
device_reply (GUPnPServiceProxy *proxy, GUPnPServiceProxyAction *action, gpointer user_data)
{
	char *baseUrl = NULL;
	GError *error = NULL;
	ProxyMapping *pm = (ProxyMapping *) user_data;
	g_return_if_fail (pm->action == action);
	pm->action = NULL;
	if (gupnp_service_proxy_end_action (proxy, action, &error, "BaseUrl", G_TYPE_STRING, &baseUrl,  NULL))
	{
		if ((error==NULL) && (baseUrl!=NULL))
		{

			if (g_list_find_custom (xdevlist, baseUrl, (GCompareFunc)strcmp) == NULL)
			{
				const gchar* udn = gupnp_service_info_get_udn (GUPNP_SERVICE_INFO (pm->proxy));

				gchar* wbdevice = NULL;
				if (g_strrstr(udn,"uuid:"))
				{
					wbdevice = g_strndup(udn+5, strlen(udn)-5);
				}

				if (g_list_str_contains(baseUrl, wbdevice)==1)
				{
					g_print("Could not add the device %s to the list - UUID does not match with Url\n",udn);
				}
				else
				{
					g_mutex_lock(mutex);
					xdevlist = g_list_append(xdevlist, baseUrl);
					g_mutex_unlock(mutex);
				}
			}
		}
		else
		{
	//		g_printerr ("Error: %s\n", error->message);
			g_error_free (error);
		}
	}
	else
	{
		const gchar* udn = gupnp_service_info_get_udn (GUPNP_SERVICE_INFO (pm->proxy));
		gchar* wbdevice = NULL;
		if (g_strrstr(udn,"uuid:"))
		{
			wbdevice = g_strndup(udn+5, strlen(udn)-5);
		}

		GList* lstXdev = g_list_find_custom (xdevlist, wbdevice, (GCompareFunc)g_list_str_contains);

		if (lstXdev)
		{
			g_mutex_lock(mutex);
			xdevlist = g_list_delete_link(xdevlist, lstXdev);
			g_mutex_unlock(mutex);
		}
		//printf("Length of xdevlist in cb is %d\n", g_list_length(xdevlist));

		if (error!=NULL)
		{
		//	g_printerr ("Error: %s\n", error->message);
			g_return_if_fail (error);
		}
	}

	g_clear_error (&error);
}

static void
device_down_cb(gpointer data)
{
    static int i = 0;
    i++;
    g_print ("timeout_callback called %d times\n",i);
}

//Discovery functions

void*
discover_devices(gpointer data)
{
	//GError *error = NULL;
	DiscoverThreadData *discThrData = (DiscoverThreadData *) data;
	char* upnpResults = "Test Results\n";
	int resultsLength = strlen(upnpResults);
	GUPnPControlPoint* upnpcp = NULL;
	

	gboolean isActive;
	guint listLength = 0;
	//g_print("Interface name is %s Port is %d\n", discThrData->upnpIfName, discThrData->upnpPort);
	/* Test Code
	//discThrData->OnStart(2);
	//discThrData->OnResultsUpdated(upnpResults, listLength);
	 Test Code */


	/*if (error!=NULL)
	{
		g_printerr ("Error: %s\n", error->message);
		g_return_if_fail (error);
	}*/

	upnpcp = gupnp_control_point_new(context, "urn:schemas-upnp-org:service:DiscoverFriendlies:1");

    //if (upnpcp==NULL)

    //g_signal_connect (upnpcp, "service-proxy-available", G_CALLBACK (service_proxy_available_cb), NULL);
	gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (upnpcp), TRUE);


	//Runs discovery thread until stopDiscovery is called
	while(discThreadFlag == 1)
	{

		//isActive = gssdp_resource_browser_get_active(GSSDP_RESOURCE_BROWSER (upnpcp));
		//g_print("Resource browser is %d Target is %s\n",isActive, gssdp_resource_browser_get_target (GSSDP_RESOURCE_BROWSER (upnpcp)));

		const GList *constLstProxies = gupnp_control_point_list_service_proxies(upnpcp);
		GList* lstProxies = g_list_first(constLstProxies);

		//g_print("Size of the list is %d", g_list_length(lstProxies));
		while(lstProxies && (g_list_length(lstProxies) > 0))
		{
			//g_print("\nIn upnp action...\n");
			ProxyMapping *pm = g_slice_new0 (ProxyMapping);
			pm->proxy = (GUPnPServiceProxy *) lstProxies->data;
			//GError *error = NULL;
			pm->action = gupnp_service_proxy_begin_action (pm->proxy, "GetBaseUrl", device_reply, pm, NULL);
			pm->timeout_src = g_timeout_source_new_seconds(5);
			g_source_set_callback (pm->timeout_src, device_down_cb, pm->proxy, NULL);
			//g_source_attach (pm->timeout_src, main_context);
			lstProxies = g_list_next(lstProxies);

		}
		//printf("Length of xdevlist in thread is %d, length of mylist is %d\n", g_list_length(xdevlist), listLength);

		if (g_list_length(xdevlist) != listLength)
		{
			listLength = g_list_length(xdevlist);
			GList *element;
			element = g_list_first(xdevlist);
                        GString *device_list = g_string_new(NULL);

			//g_print("\n{\n\t\"XCalibur_MDVR\":\n\t[\n\t\t");
			g_string_printf(device_list,"{\n\t\"XCalibur_MDVR\":\n\t[\n\t\t");

			while(element)
			{
				//g_print("{\"url\":\"%s\"}", (char *)element->data);
				g_string_append_printf(device_list,"{\"url\":\"%s\"}",(char *)element->data);

				element = g_list_next(element);
				if (element)
				{
					//g_print(",\n\t\t");
					g_string_append_printf(device_list,",\n\t\t");
				}
			}

			//g_print("\n\t]\n}");
			g_string_append_printf(device_list,"\n\t]\n}");

			discThrData->OnResultsUpdated(device_list->str, strlen(device_list->str));

			g_list_free(element);
                        g_string_free(device_list, TRUE);
		}

		g_list_free(lstProxies);
                //g_print("\nWaiting Discovery...\n");

		usleep(2000000);

	}

	//g_free(context);
	gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (upnpcp), FALSE);
	g_object_unref (upnpcp);

	g_mutex_lock(mutex);
	discThreadFlag = 1;
	g_mutex_unlock(mutex);

	g_print("Quitting discovery thread...\n");
}



//Utility Functions

//Gets IP address from a given network interface
/*
int getipaddress(const char* ifname, char* ipAddressBuffer)
{
    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;
    getifaddrs(&ifAddrStruct);
	//char addressBuffer[INET_ADDRSTRLEN] = NULL;
	int found=0;

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
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

    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
		return found;
}
*/

static gint
g_list_str_contains(gconstpointer* baseUrl, gconstpointer* udn )
{
	//g_print("Base url %s, udn is % s\n", (gchar *)baseUrl, (gchar *)udn );
	if (g_strrstr((gchar *)baseUrl, (gchar *)udn) != NULL)
		return 0;
	else
		return 1;
}
