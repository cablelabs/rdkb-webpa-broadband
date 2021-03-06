/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
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
 * @file main.c
 *
 * @description This file defines WebPA's main function
 *
 */
#include "nopoll.h"
#include "wal.h"
#include "websocket_mgr.h"
#include "stdlib.h"
#include "signal.h"

#ifdef INCLUDE_BREAKPAD
#include "breakpad_wrapper.h"
#endif

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static void __terminate_listener(int value);
static void sig_handler(int sig);

extern WAL_STATUS WebpaRpcInit();

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/

int main()
{
	// Initialize logger

	LOGInit();
	if (syscfg_init() != 0)
        {
        	WalInfo("syscfg init failure\n");
        	return 1;
        }
#ifdef INCLUDE_BREAKPAD
    breakpad_ExceptionHandler();
#else

	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGUSR2, sig_handler);
	signal(SIGSEGV, sig_handler);
	signal(SIGBUS, sig_handler);
	signal(SIGKILL, sig_handler);
	signal(SIGFPE, sig_handler);
	signal(SIGILL, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGALRM, sig_handler);
#endif

	const char *pComponentName = WEBPA_COMPONENT_NAME;
	WalInfo("********** Starting component: %s **********\n ", pComponentName); 

	msgBusInit(pComponentName);
  
	WebpaRpcInit();
	createSocketConnection();

	while(1);
	return 1;
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
static void __terminate_listener(int value) {
	terminateSocketConnection();
	return;
}
static void sig_handler(int sig)
{

	if ( sig == SIGINT ) {
		signal(SIGINT, sig_handler); /* reset it to this function */
		WalInfo("WEBPA SIGINT received!\n");
		//exit(0);
	}
	else if ( sig == SIGUSR1 ) {
		signal(SIGUSR1, sig_handler); /* reset it to this function */
		WalInfo("WEBPA SIGUSR1 received!\n");
	}
	else if ( sig == SIGUSR2 ) {
		WalInfo("WEBPA SIGUSR2 received!\n");
	}
	else if ( sig == SIGCHLD ) {
		signal(SIGCHLD, sig_handler); /* reset it to this function */
		WalInfo("WEBPA SIGHLD received!\n");
	}
	else if ( sig == SIGPIPE ) {
		signal(SIGPIPE, sig_handler); /* reset it to this function */
		WalInfo("WEBPA SIGPIPE received!\n");
	}
	else if ( sig == SIGALRM ) {
		signal(SIGALRM, sig_handler); /* reset it to this function */
		WalInfo("WEBPA SIGALRM received!\n");
	}
	else {
		WalPrint("sig_handler sig :%d received\n", sig);
		__terminate_listener(sig);
		WalInfo("WEBPA Signal %d received!\n", sig);
		exit(0);
	}
	
}
