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
#ifndef ULISTEN_H
#include "ulisten.h"
#endif

#ifndef LIBXDISC_H
#include "libxdisc.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

void OnStart(int statusCode)
{
	printf("In OnStarted with status code %d\n", statusCode);
	return;
}

void OnError(int errorCode)
{
	printf("In OnError with error code %d\n", errorCode);
	return;
}

void OnResultsUpdate(char* upnpResults, int length)
{
	//printf("Updated Device list is \n%s", upnpResults);
	printf("List length is %d\n", length);
	char results[length+1];
	strncpy(results,upnpResults,length);
	printf("Updated device list is \n%s\n");
	return;
}

void OnStop(void)
{
	printf("In OnStopped\n");
	return;
}

int main (int argc, char *argv[])
{
	if (argc < 3)
	{
		fprintf(stderr, "Error in arguments\nUsage: %s Network_interface_name port)\n", argv[0]);
	    return 1;
	}
        GMainLoop *main_loop = NULL;
        
	const char *ifName =  argv[1];
	const unsigned int port = atoi(argv[2]);
	printf("Port number is %u\n",port);
        char userSelection;
        int loopFlag = 1;
        startUpnpDiscovery(ifName, port, OnStart, OnError, OnResultsUpdate);
        
        main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (main_loop);

	return 0;
}


