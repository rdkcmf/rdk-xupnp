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
#include "fcgi_config.h"
#include "fcgiapp.h"
#include <glib.h>
#include <libgupnp/gupnp-control-point.h>
#include <string.h>
#include <unistd.h>
#define G_LOG_DOMAIN "XUPNP"
#define XUPNP_RESCAN_INTERVAL 10000000

struct ProxyMapping {
  GUPnPServiceProxy *proxy;
  GUPnPServiceProxyAction *action;
  GSource *timeout_src;
};

GList* xdevlist= NULL;
//const char* host_ip = "eth0";
const guint host_port = 50757;

GMutex *mutex;
GCond *cond;
GMainContext *main_context;
GUPnPContext *context;
GUPnPControlPoint* cp;


void* print_devices();
void* verify_devices();

void xupnp_logger (const gchar *log_domain, GLogLevelFlags log_level,
    const gchar *message, gpointer user_data);


static gint
g_list_str_contains(gconstpointer* baseUrl, gconstpointer* udn )
{
	if (g_strrstr((gchar *)baseUrl, (gchar *)udn) != NULL)
		return 0;
	else
		return 1;
}


static void
device_down_cb(gpointer data)
{
    static int i = 0;
    i++;
    g_print ("timeout_callback called %d times\n",i);
    return;
}

static void
device_proxy_available_cb (GUPnPControlPoint *cp, GUPnPDeviceProxy *dproxy, gpointer user_data)
{
    //g_print("Found a new device\n");
    g_message("Found a new gateway device");
    gchar *baseUrl = NULL;
    GError *error = NULL;
    GUPnPServiceInfo *sproxy = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dproxy), "urn:schemas-upnp-org:service:DiscoverFriendlies:1");
    //gupnp_service_proxy_send_action (sproxy, "GetBaseUrl", &error,NULL,"BaseUrl",G_TYPE_STRING, gwData->baseurl ,NULL);
    const char* udn = gupnp_device_info_get_udn(GUPNP_DEVICE_INFO (dproxy));
    if (udn != NULL)
    {
        if (g_strrstr(udn,"uuid:"))
        {
            gchar* receiverid = g_strndup(udn+5, strlen(udn)-5);
            g_message("gateway device receiver id is %s",receiverid);
            GList* lstXdev = g_list_find_custom (xdevlist, receiverid, (GCompareFunc)g_list_str_contains);
            if (lstXdev)
            {
                //g_print("Removing the old device\n");
                g_message("Removing existing device with id %s",receiverid);
                g_mutex_lock(mutex);
                xdevlist = g_list_delete_link(xdevlist, lstXdev);
                g_mutex_unlock(mutex);
            }
        } 
    }
    //g_print("Before sending action\n");
    gboolean retVal = gupnp_service_proxy_send_action (sproxy, "GetBaseUrl", &error,NULL,"BaseUrl",G_TYPE_STRING, &baseUrl, NULL);
    //g_print("After sending action\n");
    if (retVal == TRUE)
    //if (gupnp_service_proxy_end_action (proxy, action, &error, "BaseUrl", G_TYPE_STRING, &baseUrl,  NULL))
	{
        //g_print("Send action is success\n");
        
        if ((error==NULL) && (baseUrl!=NULL))
		{

			if (g_list_find_custom (xdevlist, baseUrl, (GCompareFunc)strcmp) == NULL)
			{
                //g_print("Adding base url\n");
                g_message("Adding Gateway device with base url: %s",baseUrl);
                g_mutex_lock(mutex);
				xdevlist = g_list_append(xdevlist, baseUrl);
				g_mutex_unlock(mutex);
			}
			else
			{
                		g_message("Already there : Gateway device with base url: %s",baseUrl);
				
			}
		}
		else
		{
	//		g_printerr ("Error: %s\n", error->message);
            g_critical("Error Occured while triggering the action - %s", error->message);
			g_error_free (error);
		}
	}
    else
    {
        g_critical("Send action failed");
        //g_print("Send action failed\n");
    }
}

//static void
//device_proxy_unavailable_cb (GUPnPControlPoint *cp, GUPnPDeviceProxy *dproxy, gpointer user_data)
//{
//    g_print("A device went down\n");
//}


void xupnp_logger (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
  
  const gchar* logfilename = (gchar *)user_data;
  if (logfilename != NULL)
  {
      FILE *logfile = g_fopen (logfilename, "a");
      GTimeVal timeval;
      char *timestr;
      g_get_current_time(&timeval);
      if (logfile == NULL)
      {
        // Don't fallback to console as it would corrupt fcgi output 
        //g_print ("%s: g_time_val_to_iso8601(&timeval): %s\n", message);
        return;
      }

      timestr = g_time_val_to_iso8601(&timeval);
      g_fprintf (logfile, "%s : %s\n", timestr, message);
      g_free (timestr);
      fclose (logfile);
  }
  return;
}


int main(int argc, char *argv[])
{
    //preamble
	g_thread_init (NULL);
	g_type_init();
	GMainLoop *main_loop;

    GError* error = 0;
	GThread *thread;
	//int id;
	mutex = g_mutex_new ();
	cond = g_cond_new (); 

	const char* ifname = getenv("XCAL_DEF_INTERFACE");
    const char* logoutfile = getenv("XDISC_LIST_LOG_FILE");

    FCGX_Init();
    g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_INFO | G_LOG_LEVEL_MESSAGE | \
        G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_ERROR, xupnp_logger, logoutfile);
    g_message("Starting xdiscovery http service on interface %s", ifname);
    
#ifdef GUPNP_0_19
    context = gupnp_context_new (NULL, ifname, host_port, &error);
    main_loop = g_main_loop_new (NULL, FALSE);
#else
    main_context = g_main_context_new();
    context = gupnp_context_new (main_context, ifname, host_port, &error);
    main_loop = g_main_loop_new (main_context, FALSE);
#endif
    
	if (error) {
    	g_printerr ("Error creating the GUPnP context: %s\n", error->message);
        g_critical("Error creating the XUPnP context on %s:%s Error:%s", ifname, host_port, error->message);
    	g_error_free (error);
    	return EXIT_FAILURE;
  	}
    gupnp_context_set_subscription_timeout(context, 5);

    //cp = gupnp_control_point_new(context, "urn:schemas-upnp-org:service:DiscoverFriendlies:1");
    cp = gupnp_control_point_new(context, "urn:schemas-upnp-org:device:BasicDevice:1");
    g_signal_connect (cp,"device-proxy-available", G_CALLBACK (device_proxy_available_cb), NULL);
    //g_signal_connect (cp,"device-proxy-unavailable", G_CALLBACK (device_proxy_unavailable_cb), NULL);
    gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);


    thread = g_thread_create(verify_devices, NULL,FALSE, NULL);
    g_message("Started discovery on interface %s", ifname);
    thread = g_thread_create(print_devices, NULL,TRUE, NULL);
	g_main_loop_run (main_loop);
    g_main_loop_unref (main_loop);
    g_object_unref (cp);
    g_object_unref (context);
    return 0;
}

void* verify_devices()
{
    guint lenPrevDevList = 0;
    guint lenCurDevList = 0;
    
    while(1)
	{
        //g_main_context_push_thread_default(main_context);
        //g_message("Before rescan");
        if (gssdp_resource_browser_rescan(GSSDP_RESOURCE_BROWSER(cp))==FALSE)
        {
            //g_print("Forced rescan failed\n");
            //g_message("Forced rescan failed, sleeping");
            usleep(XUPNP_RESCAN_INTERVAL);
            continue;
        }
        //else
        //    g_message("Forced rescan success\n");
        //g_debug("Forced rescan success");
        const GList *constLstProxies = gupnp_control_point_list_device_proxies(cp);

        if (NULL == constLstProxies)
        {
          //g_message("Error in getting list of device proxies or No device proxies found");
          lenCurDevList = 0;
        }
        else
          lenCurDevList = g_list_length(constLstProxies);

        //g_message("Current list length is %u\n",lenCurDevList);
        
        //Some device went down, fix it in the list
        if ((lenCurDevList < lenPrevDevList) && lenCurDevList > 0)
        {
            g_message("Device List length reduced - Current length: %u - Previous Length: %u",lenCurDevList, lenPrevDevList);
            //Loop through all devices in xdevlist
            if (g_list_length(xdevlist) > 0)
            {
                gboolean existsFlag = FALSE;
                GList *element;
                element = g_list_first(xdevlist);
                while(element)
                {
                   //Loop through all new found devices
                    GList* lstProxies = g_list_first(constLstProxies);
                    g_message("Length of lstProxies is %u", g_list_length(lstProxies));
                    existsFlag = FALSE;
                    while(lstProxies)
                    {
                        //Check if this returns NULL and skip rest
                        const gchar* udn = gupnp_device_info_get_udn (GUPNP_DEVICE_INFO((GUPnPDeviceProxy *)lstProxies->data));

                        if ((NULL!=udn) && (element))
                        {
                            const gchar* receiverid = g_strndup(udn+5, strlen(udn)-5);

                           //Matched the uuid (whitebox id) - No need to do anything - quit the loop
                           if (NULL!=strstr(g_strstrip((gchar *)element->data), g_strstrip(receiverid)))
                           {
                               existsFlag=TRUE;
                               g_message("Found udn %s, Quitting loop",receiverid);
                               break;
                           }
                           //There are still elements to search
                           g_message("Not Found %s in %s, going to next element",g_strstrip(receiverid), g_strstrip((gchar *)element->data));
                        }
                        

                        lstProxies = g_list_next(lstProxies);
                                 
                    }
                    //g_list_free(lstProxies);
                    //End - Loop through all new found devices
                    
                    //Device not found in new device list. Mark it for deletion later.
                    if ((existsFlag==FALSE) && (!((g_list_previous(element) == NULL) && (g_list_next(element) == NULL))))
                    {
                        gchar *baseUrl = (gchar *) element->data;
                        element = g_list_next(element);
                        g_mutex_lock(mutex);
                        g_message("Removing %s \n", baseUrl);
                        xdevlist = g_list_remove(xdevlist, baseUrl);
                        if (NULL == baseUrl)
                        {
                        	g_message("existing flag is false : base url is empty");
                        }
                        else
                        {
	                        g_free(baseUrl);
                        }
                        g_mutex_unlock(mutex);
                    }
                    else
                    {
                    //There are still elements to search
                    	element = g_list_next(element); 
                    }
                }
                //g_list_free(element);
            }
            //End - Loop through all devices in xdevlist

        }
        else if (lenCurDevList == 0)
        {
            //g_message("Current list length3 is %u\n",lenCurDevList);
            g_message("Empty Device List - Current length: %u - Previous Length: %u",lenCurDevList, lenPrevDevList);
            if (g_list_length(xdevlist) > 0)
            {
                GList *element;
                element = g_list_first(xdevlist);
                while(element)
                {
                    gchar *baseUrl = (gchar *) element->data;  
					element = g_list_next(element);                      
                    g_mutex_lock(mutex);
                    xdevlist = g_list_remove(xdevlist, baseUrl);
                    //g_print("Element Removed\n");
                    if (NULL == baseUrl)
                    {
                    	g_message("base url is empty");
                    }
                    else
                    {
                    	g_free(baseUrl);
                    }
                    g_mutex_unlock(mutex);
                     
                }
            }
            
        }
        else if ((lenCurDevList > lenPrevDevList) && lenCurDevList > 0)
        {
            //Loop through all new found devices
            g_message("Device List length increase - Current length: %u - Previous Length: %u",lenCurDevList, lenPrevDevList);
           
            GList* lstProxies = g_list_first(constLstProxies);
            while(lstProxies)
            {
                const gchar* udn = gupnp_device_info_get_udn (GUPNP_DEVICE_INFO((GUPnPDeviceProxy *)lstProxies->data));
                gchar* wbdevice = NULL;
                if (g_strrstr(udn,"uuid:"))
                {
                    wbdevice = g_strndup(udn+5, strlen(udn)-5);
                }

                //Device not found in new device list. Mark it for deletion later.
                if (NULL != wbdevice)
                {
                    if (g_list_find_custom (xdevlist, wbdevice, (GCompareFunc)g_list_str_contains) == NULL)
                        device_proxy_available_cb(cp, (GUPnPDeviceProxy *)lstProxies->data, NULL);
                }

                //There are still elements to search
                lstProxies = g_list_next(lstProxies);         
            }
            //g_list_free(lstProxies);
            
            //g_print("Current list is greater\n");
        }

        //After cleaning up, update the prev list length
        lenPrevDevList=lenCurDevList;
		//g_print("\nWaiting Discovery...\n");
        usleep(XUPNP_RESCAN_INTERVAL);
		
	}
}



void* print_devices()
{
	FCGX_Request request;

	while (1)
	{
		GList *element;
		element = g_list_first(xdevlist);

		FCGX_InitRequest(&request, 0, 0);
		int rc = FCGX_Accept_r(&request);
		if (rc < 0) break;
		FCGX_FPrintF(request.out, "Status: 200 OK\r\nContent-Type: text/plain\r\n\r\n");

		FCGX_FPrintF(request.err, "Length of the list is %d\n", g_list_length(xdevlist));

		element = g_list_first(xdevlist);

		FCGX_FPrintF(request.out, "\n{\n\t\"XCalibur_MDVR\":\n\t[\n\t\t");
		while(element)
		{
			FCGX_FPrintF(request.out, "{\"url\":\"%s\"}", (char *)element->data);
                        g_message("Sending Data to requestor: <%s>", (char *)element->data);
                        element = g_list_next(element);
			if (element)
				FCGX_FPrintF(request.out, ",\n\t\t");
        }
		g_list_free(element);
		FCGX_FPrintF(request.out, "\n\t]\n}");
        FCGX_Finish_r(&request);
        }
    return NULL;
}
