
#include "ansc_platform.h"
#include "cosa_Webpa_dml.h"
#include "ccsp_trace.h"
#include "msgpack.h"
#include "base64.h"
#include "wal.h"

//static WebPA_Dispatcher message_dispatch_callback = NULL;
void (*notifyCbFnPtr)(NotifyData*) = NULL;
void sendUpstreamNotification(char *UpstreamMsg, int size);

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Webpa_SetParamValues
            (
                ANSC_HANDLE                 hInsContext,
                char**                      ppParamArray,
                PSLAP_VARIABLE*             ppVarArray,
                ULONG                       ulArraySize,
                PULONG                      pulErrorIndex
            );

    description:

        This function is called to set bulk parameter values; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char**                      ppParamName,
                The parameter name array;

                PSLAP_VARIABLE*             ppVarArray,
                The parameter values array;

                ULONG                       ulArraySize,
                The size of the array;

                PULONG                      pulErrorIndex
                The output parameter index of error;

    return:     TRUE if succeeded.

**********************************************************************/

BOOL
Webpa_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
	char*                       pString
    )
{
	msgpack_zone mempool;
	msgpack_object deserialized;
	
	msgpack_unpack_return unpack_ret;
	char * decodeMsg =NULL;
	int decodeMsgSize =0;
	int size =0;
	WalPrint("<========= Start of Webpa_SetParamStringValue ========>\n");
	WalInfo("Received data ParamName %s,data length: %d bytes, Value : %s\n",ParamName, strlen(pString), pString);
	
	if( AnscEqualString(ParamName, "PostData", TRUE))
    	{    		
    		//Start of b64 decoding
    		WalPrint("----Start of b64 decoding----\n");
    		decodeMsgSize = b64_get_decoded_buffer_size(strlen(pString));
    		WalPrint("expected b64 decoded msg size : %d bytes\n",decodeMsgSize);
    		
		decodeMsg = (char *) malloc(sizeof(char) * decodeMsgSize);
				
    		size = b64_decode( pString, strlen(pString), decodeMsg );
    		WalPrint("base64 decoded data containing %d bytes is :%s\n",size, decodeMsg);
    		
    		WalPrint("----End of b64 decoding----\n");
    		//End of b64 decoding
    		
    		//Start of msgpack decoding just to verify
    		WalPrint("----Start of msgpack decoding----\n");
		msgpack_zone_init(&mempool, 2048);
		unpack_ret = msgpack_unpack(decodeMsg, size, NULL, &mempool, &deserialized);
		WalPrint("unpack_ret is %d\n",unpack_ret);
		switch(unpack_ret)
		{
			case MSGPACK_UNPACK_SUCCESS:
				WalInfo("MSGPACK_UNPACK_SUCCESS :%d\n",unpack_ret);
				WalPrint("\nmsgpack decoded data is:");
				msgpack_object_print(stdout, deserialized);
			break;
			case MSGPACK_UNPACK_EXTRA_BYTES:
				WalError("MSGPACK_UNPACK_EXTRA_BYTES :%d\n",unpack_ret);
			break;
			case MSGPACK_UNPACK_CONTINUE:
				WalError("MSGPACK_UNPACK_CONTINUE :%d\n",unpack_ret);
			break;
			case MSGPACK_UNPACK_PARSE_ERROR:
				WalError("MSGPACK_UNPACK_PARSE_ERROR :%d\n",unpack_ret);
			break;
			case MSGPACK_UNPACK_NOMEM_ERROR:
				WalError("MSGPACK_UNPACK_NOMEM_ERROR :%d\n",unpack_ret);
			break;
			default:
				WalError("Message Pack decode failed with error: %d\n", unpack_ret);	
		}

		msgpack_zone_destroy(&mempool);
		WalPrint("----End of msgpack decoding----\n");
		//End of msgpack decoding
		
		notifyCbFnPtr = getNotifyCB();
		if (NULL == notifyCbFnPtr) {
			WalError("Fatal: notifyCbFnPtr is NULL\n");
			return FALSE;
		}
		else
		{
			WalPrint("before sendUpstreamNotification in cosaDml\n");
			sendUpstreamNotification(decodeMsg, size);
			WalPrint("After sendUpstreamNotification in cosaDml\n");
		}
		WalPrint("----End PostData parameter----\n");

		return TRUE;
	}         
	WalPrint("<=========== End of Webpa_SetParamStringValue ========\n");

    return FALSE;
}

/**
 * @brief sendUpstreamNotification function to send Upstream notification
 *
 * @param[in] UpstreamMsg Upstream notification message
 * @param[in] size Size of Upstream notification message
 */
void sendUpstreamNotification(char *msg, int size)
{
	WalPrint("Inside sendUpstreamNotification msg length %d\n", size);

	// create NotifyData struct
	UpstreamMsg *msgPtr = (UpstreamMsg *) malloc(sizeof(UpstreamMsg) * 1);
	msgPtr->msg = msg;
	msgPtr->msgLength = size;
	NotifyData *notifyDataPtr = (NotifyData *) malloc(sizeof(NotifyData) * 1);
	notifyDataPtr->type = UPSTREAM_MSG;
	Notify_Data *notify_data = (Notify_Data *) malloc(sizeof(Notify_Data) * 1);
	notify_data->msg = msgPtr;
	notifyDataPtr->data = notify_data;

	if (NULL != notifyCbFnPtr) 
	{
		(*notifyCbFnPtr)(notifyDataPtr);
	}
}
