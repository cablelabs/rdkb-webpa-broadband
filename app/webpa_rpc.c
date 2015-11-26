/**
 * @file webpa_rpc.c
 *
 * @description This file defines WebPA RPC functions
 *
 * Copyright (c) 2015  Comcast
 */
#include "wal.h"
#include <pthread.h>

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define MAX_RPC_MSG_LEN			256
//#define WEBPA_ENABLE_RPC

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
/* none */

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
static pthread_mutex_t rpcSendReceiveMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t rpcSendReceiveCond = PTHREAD_COND_INITIALIZER;
static char* rpcResponse = NULL;

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
void receiveRpcMsgCB(int n, char const* buff);

extern int WebPA_ClientConnector_Start();
//extern int WebPA_ClientConnector_SetDispatchCallback(WebPA_ClientConnector_Dispatcher callback);
extern int WebPA_ClientConnector_DispatchMessage(char const* topic, char const* buff, int n);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
#if 0
//function to test echo message
static void sendMessage(void)
{
	int len = 0;
	char buff[512];
	int cnt = 0; 
  
	while(1)
	{
		memset(buff, 0, sizeof(buff));
		sprintf(buff, "WebpaServer send message %d ",cnt++);
		len = strlen(buff);
		buff[len-1] = '\0';
		WebPA_ClientConnector_DispatchMessage("iot", buff, len);
		sleep(5);
		if(cnt > 256)
		{
			cnt = 0;
    		}		
  	}
}
#endif
/**
 * @brief Initializes WebPA RPC.
 *
 * @return WAL_STATUS.
 */
WAL_STATUS WebpaRpcInit()
{
#if 0
	//pthread_t sender;
	WebPA_ClientConnector_SetDispatchCallback(&receiveRpcMsgCB);
	WebPA_ClientConnector_Start();
	printf("Successfully initialized WebPA RPC\n");

	//pthread_create(&sender, NULL,&sendMessage, NULL);
#else
	WalInfo("WebPA RPC interface is disabled\n");
#endif
	return WAL_SUCCESS;
}

/**
 * @brief sendIoTMessage interface sends message to IoT. 
 *
 * @param[in] msg Message to be sent to IoT.
 * @param[out] ret Response from IoT.
 */
void sendIoTMessage(void *msg, void *ret)
{
#ifdef WEBPA_ENABLE_RPC
	if(msg != NULL)
	{
		WebPA_ClientConnector_DispatchMessage("iot", (char*)msg, strlen(msg));
	}
	else
	{
		WalError("sendIoTMessage: Null Message");
	}

	pthread_mutex_lock(&rpcSendReceiveMutex);
	while(!rpcResponse)
	{
		WalInfo("sendIoTMessage() is waiting on rpcSendReceiveCond\n");
		pthread_cond_wait(&rpcSendReceiveCond, &rpcSendReceiveMutex);
	}

	ret = malloc(MAX_RPC_MSG_LEN);
	strncpy((char *)ret, rpcResponse, MAX_RPC_MSG_LEN);
	((char*)ret)[MAX_RPC_MSG_LEN-1] = '\0';
	WAL_FREE(rpcResponse);
	rpcResponse = NULL;
	pthread_mutex_unlock(&rpcSendReceiveMutex);

	WalInfo("sendIoTMessage() ret: [%zu]%s\n", strlen((char*)ret), (char*)ret);
#endif
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/

void receiveRpcMsgCB(int n, char const* buff)
{
#ifdef WEBPA_ENABLE_RPC
	WalInfo("Received message: '%.*s'\n", n, buff);
	pthread_mutex_lock(&rpcSendReceiveMutex);
	rpcResponse = buff;
	pthread_cond_signal(&rpcSendReceiveCond);
	pthread_mutex_unlock(&rpcSendReceiveMutex);
#endif
}
