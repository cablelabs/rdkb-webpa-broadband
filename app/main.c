/**
 * @file main.c
 *
 * @description This file defines WebPA's main function
 *
 * Copyright (c) 2015  Comcast
 */
#include "nopoll.h"
#include "wal.h"
#include "websocket_mgr.h"
#include "stdlib.h"
#include "signal.h"

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static void __terminate_listener(int value);
static void sig_handler(int sig);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
int main()
{
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

	const char *pComponentName = WEBPA_COMPONENT_NAME;
	WalInfo("********** Starting component: %s **********\n ", pComponentName); 
	msgBusInit(pComponentName);
	WALInit();
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
	else if( sig == SIGTERM ) {
		signal(SIGTERM, __terminate_listener);
		WalInfo("WEBPA SIGTERM received!\n");
		exit(0);
	}
	else {
		WalInfo("WEBPA Signal %d received!\n", sig);
		exit(0);
	}
}
