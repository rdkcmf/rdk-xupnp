/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
#include <libgupnp/gupnp.h>
#include <stdio.h>
#include <stdlib.h>
#include <gmodule.h>
#include <stdbool.h>
#include <memory.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <string.h>
#include "secure_wrapper.h"
#include "xdevice.h"
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <libgupnp/gupnp-control-point.h>
#ifdef ENABLE_SD_NOTIFY
#include <systemd/sd-daemon.h>
#endif
#include "rdk_safeclib.h"
#define SERVER_CONTEXT_PORT 50769
#define DEVICE_PROTECTION_CONTEXT_PORT  50761
#define IDM_SERVICE "urn:schemas-upnp-org:service:X1IDM:1"
#define IDM_DP_SERVICE "urn:schemas-upnp-org:service:X1IDM_DP:1"
#define IDM_CERT_FILE "/tmp/idm_xpki_cert"
#define IDM_KEY_FILE "/tmp/idm_xpki_key"
#define IDM_CA_FILE "/tmp/idm_UPnP_CA"
#ifndef _GNU_SOURCE
 #define _GNU_SOURCE
#endif
#define MAC_ADDR_SIZE 18
#define IPv4_ADDR_SIZE 16
#define IPv6_ADDR_SIZE 128
#define ACCOUNTID_SIZE 30
char clientIp[IPv4_ADDR_SIZE],bcastMacaddress[MAC_ADDR_SIZE],gwyIpv6[IPv6_ADDR_SIZE],interface[IPv4_ADDR_SIZE],accountId[ACCOUNTID_SIZE],uUid[256];
GString* bcastmacaddress,*serial_num,*recv_id;
void free_server_memory();
int check_file_presence();
BOOL check_empty_idm(char *str)
{
    if (str[0]) {
        return TRUE;
    }
    return FALSE;
}
bool check_null_idm(char *str)
{
    if (str) {
        return true;
    }
    return false;
}

gchar *getmacaddress_idm(const gchar *ifname)
{
    errno_t rc = -1;
    int fd;
    struct ifreq ifr;
    unsigned char *mac;
    GString *data = g_string_new(NULL);
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;
    rc = strcpy_s(ifr.ifr_name,IFNAMSIZ - 1,ifname);
    ERR_CHK(rc);
    ioctl(fd, SIOCGIFHWADDR, &ifr);
    close(fd);
    mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    //display mac address
    g_string_printf(data, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac[0], mac[1], mac[2],
                    mac[3], mac[4], mac[5]);
    return data->str;
}

BOOL getBcastMacAddress_idm(char *outValue)
{
    BOOL result = FALSE;
    errno_t rc = -1;
    if (!check_null_idm(outValue)) {
        g_message("getBcastMacAddress_idm : NULL string !");
        return result;
    }
    if ( interface != NULL ) {
        const gchar *bcastmac = (gchar *)getmacaddress_idm(interface);
        if (bcastmac) {
            g_message("Broadcast MAC address in  interface: %s  %s ",interface,bcastmac);
        }
        else
        {
            g_message("failed to retrieve macaddress on interface %s ", interface);
            return result;
        }
        bcastmacaddress = g_string_new(NULL);
        g_string_assign(bcastmacaddress, bcastmac);
        g_message("bcast mac address is %s", bcastmacaddress->str);
        rc = strcpy_s(outValue,MAC_ADDR_SIZE,bcastmacaddress->str);
        if(rc == EOK)
        {
            result = TRUE;
        }
        else
        {
            ERR_CHK(rc);
        }
    }
    else
    {
        g_message("getBcastMacAddress_idm : Empty broadcast interface");
    }
    return result;
}

G_MODULE_EXPORT void
get_bcastmacaddress_cb (GUPnPService *service, GUPnPServiceAction *action, gpointer user_data)
{
    getBcastMacAddress_idm(bcastMacaddress);
    gupnp_service_action_set (action, "BcastMacAddress", G_TYPE_STRING, bcastMacaddress, NULL);
    gupnp_service_action_return (action);
}

G_MODULE_EXPORT void
query_bcastmacaddress_cb (GUPnPService *service, char *variable, GValue *value, gpointer user_data)
{
    g_value_init (value, G_TYPE_STRING);
    g_value_set_string (value, bcastMacaddress);
}

G_MODULE_EXPORT void
get_client_ip_cb (GUPnPService *service, GUPnPServiceAction *action, gpointer user_data)
{
    gupnp_service_action_set (action, "ClientIP", G_TYPE_STRING, clientIp, NULL);
    gupnp_service_action_return (action);
}

G_MODULE_EXPORT void
query_client_ip_cb (GUPnPService *service, char *variable, GValue *value, gpointer user_data)
{
    g_value_init (value, G_TYPE_STRING);
    g_value_set_string (value, clientIp);
}
G_MODULE_EXPORT void
get_gwyipv6_cb (GUPnPService *service, GUPnPServiceAction *action, gpointer user_data)
{
    gupnp_service_action_set (action, "GatewayIPv6", G_TYPE_STRING, gwyIpv6, NULL);
    gupnp_service_action_return (action);
}
G_MODULE_EXPORT void
query_gwyipv6_cb (GUPnPService *service, char *variable, GValue *value, gpointer user_data)
{
    g_value_init (value, G_TYPE_STRING);
    g_value_set_string (value, gwyIpv6);
}

G_MODULE_EXPORT void
get_account_id_cb (GUPnPService *service, GUPnPServiceAction *action, gpointer user_data)
{
    memset(accountId,0,ACCOUNTID_SIZE);
    getAccountId(accountId);
    g_message("accountId=%s",accountId);
    gupnp_service_action_set (action, "AccountId", G_TYPE_STRING, accountId,NULL);
    gupnp_service_action_return (action);
}

G_MODULE_EXPORT void
query_account_id_cb (GUPnPService *service, char *variable, GValue *value, gpointer user_data)
{
    g_value_init (value, G_TYPE_STRING);
    g_value_set_string (value, accountId);
}

xmlDoc * open_document(const char * file_name)
{
    xmlDoc * ret;
    ret = xmlReadFile(file_name, NULL, 0);
    if (ret == NULL)
    {
        //g_printerr("Failed to parse %s\n", file_name);
        return NULL;
    }
    return ret;
}

static xmlNode * get_node_by_name(xmlNode * node, const char *node_name)
{
    errno_t rc       = -1;
    int     ind      = -1;
    xmlNode * cur_node = NULL;
    xmlNode * ret       = NULL;
    for (cur_node = node ; cur_node ; cur_node = cur_node->next)
    {
        rc = strcmp_s(cur_node->name, strlen(cur_node->name), node_name, &ind);
        ERR_CHK(rc);
        if ((ind ==0) && (rc == EOK))
        {
            return cur_node;
        }
        ret = get_node_by_name(cur_node->children, node_name);
        if ( ret != NULL )
            break;
    }
    return ret;
}

int set_content(xmlDoc* doc, const char * node_name, const char * new_value)
{
    xmlNode * root_element = NULL;
    xmlNode * target_node = NULL;
    root_element = xmlDocGetRootElement(doc);
    target_node = get_node_by_name(root_element, node_name);
    if (target_node==NULL)
    {
        g_printerr("Couldn't locate the Target node\n");
        return 1;
    }
    xmlNodeSetContent(target_node,new_value);
    return 0;
}

BOOL updatexmldata(const char* xmlfilename, const char* struuid,const char* serialno)
{
    xmlDoc * doc = open_document(xmlfilename);
    if (doc == NULL)
    {
        g_printerr ("Error reading the Device XML file\n");
        return FALSE;
    }
    if (set_content(doc, "UDN", struuid)!=0)
    {
        g_printerr ("Error setting the unique device id in conf xml\n");
        return FALSE;
    }
    if (set_content(doc, "serialNumber", serialno)!=0)
    {
        g_printerr ("Error setting the serial number in conf xml\n");
        return FALSE;
    }
    FILE *fp = fopen(xmlfilename, "w");
    if (fp==NULL)
    {
        g_printerr ("Error opening the conf xml file for writing\n");
        return FALSE;
    }
    else if (xmlDocFormatDump(fp, doc, 1) == -1)
    {
        g_printerr ("Could not write the conf to xml file\n");
        /*Coverity Fix CID 125137,28460  RESOURCE_LEAK */
        fclose(fp);
        xmlFreeDoc(doc);

        return FALSE;
    }
    fclose(fp);
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return TRUE;
}

void free_server_memory()
{
    g_message("Inside %s",__FUNCTION__);
#ifdef IDM_DEBUG
    g_object_unref (upnpService);
    g_object_unref (baseDev);
    g_object_unref (upnpContext);
#else
    g_object_unref (upnpIdService);
    g_object_unref (dev);
    g_object_unref (upnpContextDeviceProtect);
#endif
}
BOOL getUidfromRecvId()
{
    BOOL result = FALSE;
    guint loopvar = 0;
    gchar **tokens = g_strsplit_set(bcastmacaddress->str, "':''\n'", -1);
    guint toklength = g_strv_length(tokens);
    while (loopvar < toklength)
    {
        g_string_printf(recv_id, "ebf5a0a0-1dd1-11b2-a90f-%s%s%s%s%s%s", g_strstrip(tokens[loopvar]), g_strstrip(tokens[loopvar + 1]),g_strstrip(tokens[loopvar + 2]),g_strstrip(tokens[loopvar + 3]),g_strstrip(tokens[loopvar + 4]),g_strstrip(tokens[loopvar + 5]));
        g_message("getUidfromRecvId: recvId: %s", recv_id->str);
        result = TRUE;
        break;
    }
    g_strfreev(tokens);
    return result;
}
BOOL getUUID(char *outValue)
{
    BOOL result = FALSE;
    if (!check_null_idm(outValue)) {
        g_message("getUUID : NULL string !");
        return result;
    }
    if (getUidfromRecvId()){
        if( (check_empty_idm(recv_id->str))) {
            sprintf(outValue, "uuid:%s", recv_id->str);
            result = TRUE;
        }
        else
        {
            g_message("getUUID : empty recvId");
        }
    }
    else
    {
        g_message("getUUID : could not get UUID");
    }
    return result;
}
int idm_server_start(char* Interface)
{
    g_thread_init (NULL);
    g_type_init();
    GError* error = 0;
    strcpy(interface,Interface);
    g_message("%s %d interface=%s",__FUNCTION__,__LINE__,interface);
    getipaddress((const char *)interface,clientIp,FALSE);
    serial_num = g_string_new(NULL);
    getserialnum(serial_num);
    getipaddress((const char *)interface,gwyIpv6,TRUE);
    getBcastMacAddress_idm(bcastMacaddress);
#ifndef IDM_DEBUG
    char certFile[24],keyFile[24],caFile[24]=IDM_CA_FILE;
    strcpy(certFile,IDM_CERT_FILE);
    strcpy(keyFile,IDM_KEY_FILE);
    g_message("cert file=%s  key file = %s",certFile,keyFile);
    if((access(certFile,F_OK ) == 0) && (access(keyFile,F_OK ) == 0) && (access(caFile,F_OK ) == 0))
    {
        const char* struuid_dp=g_strconcat("uuid:", g_strstrip(bcastMacaddress),NULL);
        int result = updatexmldata("/etc/xupnp/IDM_DP.xml",struuid_dp,serial_num->str);
        if (!result)
        {
            fprintf(stderr,"Failed to open the device xml file /etc/xupnp/IDM_DP.xml\n");
        }
#ifndef GUPNP_1_2
        upnpContextDeviceProtect = gupnp_context_new_s ( NULL,interface,DEVICE_PROTECTION_CONTEXT_PORT,certFile,keyFile, &error);
#else
        upnpContextDeviceProtect = gupnp_context_new_s ( interface,DEVICE_PROTECTION_CONTEXT_PORT,certFile,keyFile, &error);
#endif
        g_message("created new upnpContext");
        if (error)
        {
            g_message("%s:Error creating the Device Protection Broadcast context: %s",
                    __FUNCTION__,error->message);
            /* g_clear_error() frees the GError *error memory and reset pointer if set in above operation */
            g_clear_error(&error);
        }
        else
        {
            gupnp_context_set_subscription_timeout(upnpContextDeviceProtect, 0);
            // Set TLS config params here.
            gupnp_context_set_tls_params(upnpContextDeviceProtect,caFile,keyFile, NULL);
#ifndef GUPNP_1_2
            dev = gupnp_root_device_new (upnpContextDeviceProtect, "/etc/xupnp/IDM_DP.xml", "/etc/xupnp/");
#else
            dev = gupnp_root_device_new (upnpContextDeviceProtect, "/etc/xupnp/IDM_DP.xml", "/etc/xupnp/", &error);
#endif
            gupnp_root_device_set_available (dev, TRUE);
            upnpIdService = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (dev), IDM_DP_SERVICE);
            if (!upnpIdService)
            {
                g_message("Cannot get X1Identity service\n");
            }
            else
            {
                g_message("XUPNP Identity service successfully created");
            }
            g_signal_connect (upnpIdService, "action-invoked::GetBcastMacAddress", G_CALLBACK (get_bcastmacaddress_cb), NULL);
            g_signal_connect (upnpIdService, "query-variable::BcastMacAddress", G_CALLBACK (query_bcastmacaddress_cb), NULL);
            g_signal_connect (upnpIdService, "action-invoked::GetClientIP", G_CALLBACK (get_client_ip_cb), NULL);
            g_signal_connect (upnpIdService, "query-variable::ClientIP", G_CALLBACK (query_client_ip_cb), NULL);
            g_signal_connect (upnpIdService, "action-invoked::GetAccountId", G_CALLBACK (get_account_id_cb), NULL);
            g_signal_connect (upnpIdService, "query-variable::AccountId", G_CALLBACK (query_account_id_cb), NULL);
            g_signal_connect (upnpIdService, "action-invoked::GetGatewayIPv6", G_CALLBACK (get_gwyipv6_cb), NULL);
            g_signal_connect (upnpIdService, "query-variable::GatewayIPv6", G_CALLBACK (query_gwyipv6_cb), NULL);
        }
    }
    else
    {
        g_message("%s:mandatory files doesn't present",__FUNCTION__);
    }
#else
    recv_id=g_string_new(NULL);
    getUUID(uUid);
    const char* struuid = uUid;
    g_message("recv_id=%s",struuid);
    int result = updatexmldata("/etc/xupnp/IDM.xml",struuid,serial_num->str);
    if (!result)
    {
        fprintf(stderr,"Failed to open the device xml file /etc/xupnp/IDM.xml\n");
    }
    else
    {
        g_message("Updated the device xml file:IDM.XML uuid: %s",struuid);
    }
#ifndef GUPNP_1_2
    upnpContext = gupnp_context_new (NULL, interface, SERVER_CONTEXT_PORT, &error);
#else
    upnpContext = gupnp_context_new (interface, SERVER_CONTEXT_PORT, &error);
#endif
    if (error) {
        g_message("Error creating the Broadcast context: %s",
                error->message);
        /* g_clear_error() frees the GError *error memory and reset pointer if set in above operation */
        g_clear_error(&error);
        return 1;
    }
    gupnp_context_set_subscription_timeout(upnpContext, 0);
#ifndef GUPNP_1_2
    baseDev = gupnp_root_device_new (upnpContext, "/etc/xupnp/IDM.xml", "/etc/xupnp/");
#else
    baseDev = gupnp_root_device_new (upnpContext, "/etc/xupnp/IDM.xml", "/etc/xupnp/", &error);
#endif
    gupnp_root_device_set_available (baseDev, TRUE);
    upnpService = gupnp_device_info_get_service(GUPNP_DEVICE_INFO (baseDev), IDM_SERVICE);
    if (!upnpService)
    {
        g_printerr ("Cannot get DiscoverFriendlies service\n");
        return 1;
    }
    g_signal_connect (upnpService, "action-invoked::GetBcastMacAddress", G_CALLBACK (get_bcastmacaddress_cb), NULL);
    g_signal_connect (upnpService, "query-variable::BcastMacAddress", G_CALLBACK (query_bcastmacaddress_cb), NULL);
    g_signal_connect (upnpService, "action-invoked::GetClientIP", G_CALLBACK (get_client_ip_cb), NULL);
    g_signal_connect (upnpService, "query-variable::ClientIP", G_CALLBACK (query_client_ip_cb), NULL);
    g_signal_connect (upnpService, "action-invoked::GetGatewayIPv6", G_CALLBACK (get_gwyipv6_cb), NULL);
    g_signal_connect (upnpService, "query-variable::GatewayIPv6", G_CALLBACK (query_gwyipv6_cb), NULL);
#endif
    g_message("completed %s\n",__FUNCTION__);
    return 0;
}
