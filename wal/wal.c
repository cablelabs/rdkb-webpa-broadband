/**
 * @file wal.c
 *
 * @description This file describes the Webpa Abstraction Layer
 *
 * Copyright (c) 2015  Comcast
 */
#include "ssp_global.h"
#include "stdlib.h"
#include "ccsp_dm_api.h"
#include "wal.h"
#include <sys/time.h>

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define WIFI_INDEX_MAP_SIZE					18
#define WIFI_PARAM_MAP_SIZE					3
#define WIFI_MAX_STRING_LEN					128
#define MAX_PARAMETERNAME_LEN					512
#define MAX_PARAMETERVALUE_LEN					512
#define MAX_DBUS_INTERFACE_LEN					32
#define MAX_PATHNAME_CR_LEN					64
#define CCSP_COMPONENT_ID_WebPA					0x0000000A
#define CCSP_COMPONENT_ID_XPC					0x0000000B
#define RDKB_TR181_OBJECT_LEVEL1_COUNT			34
#define RDKB_TR181_OBJECT_LEVEL2_COUNT			12
#define WAL_COMPONENT_INIT_RETRY_COUNT			4
#define WAL_COMPONENT_INIT_RETRY_INTERVAL		10

/* WebPA Configuration for RDKB */
#define RDKB_WEBPA_COMPONENT_NAME            "com.cisco.spvtg.ccsp.webpaagent"
#define RDKB_WEBPA_CFG_FILE                  "/nvram/webpa_cfg.json"
#define RDKB_WEBPA_CFG_FILE_SRC              "/fss/gw/usr/ccsp/webpa/webpa_cfg.json"
#define RDKB_WEBPA_DEVICE_MAC                "Device.DeviceInfo.X_COMCAST-COM_CM_MAC"
#define RDKB_XPC_SYNC_PARAM_CID              "Device.DeviceInfo.Webpa.X_COMCAST-COM_CID"
#define RDKB_XPC_SYNC_PARAM_CMC              "Device.DeviceInfo.Webpa.X_COMCAST-COM_CMC"
#define RDKB_XPC_SYNC_PARAM_SPV              "Device.DeviceInfo.Webpa.X_COMCAST-COM_SyncProtocolVersion"
#define STR_NOT_DEFINED                      "Not Defined"

/* RDKB Logger defines */
#define LOG_FATAL 0
#define LOG_ERROR 1
#define LOG_WARN 2
#define LOG_NOTICE 3
#define LOG_INFO 4
#define LOG_DEBUG 5

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
typedef struct
{
	ULONG WebPaInstanceNumber;
	ULONG CcspInstanceNumber;
}CpeWebpaIndexMap;

typedef struct 
{
  int comp_id;   //Unique id for the component
  int comp_size;
  char *obj_name;
  char *comp_name;
  char *dbus_path;
}ComponentVal;

typedef struct 
{
  int parameterCount;   
  char **parameterName;
  char *comp_name;
  char *dbus_path;
}ParamCompList;

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
void (*notifyCbFn)(ParamNotify*) = NULL;
static ComponentVal ComponentValArray[RDKB_TR181_OBJECT_LEVEL1_COUNT] = {'\0'};
ComponentVal SubComponentValArray[RDKB_TR181_OBJECT_LEVEL2_COUNT] = {'\0'};
BOOL bRadioRestartEn = FALSE;
BOOL bRestartRadio1 = FALSE;
BOOL bRestartRadio2 = FALSE;
pthread_mutex_t applySetting_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t applySetting_cond = PTHREAD_COND_INITIALIZER;
int compCacheSuccessCnt = 0, subCompCacheSuccessCnt = 0;
char * wifiCompName = NULL;
static char *CcspDmlName[WIFI_PARAM_MAP_SIZE] = {"Device.WiFi.Radio", "Device.WiFi.SSID", "Device.WiFi.AccessPoint"};
static CpeWebpaIndexMap IndexMap[WIFI_INDEX_MAP_SIZE] = {
{10000, 1},
{10100, 2},
{10001, 1},
{10002, 3},
{10003, 5},
{10004, 7},
{10005, 9},
{10006, 11},
{10007, 13},
{10008, 15},
{10101, 2},
{10102, 4},
{10103, 6},
{10104, 8},
{10105, 10},
{10106, 12},
{10107, 14},
{10108, 16}
};
static char *objectList[] ={
"Device.WiFi.",
"Device.DeviceInfo.",
"Device.GatewayInfo.",
"Device.Time.",
"Device.UserInterface.",
"Device.InterfaceStack.",
"Device.Ethernet.",
"Device.MoCA.",
"Device.PPP.",
"Device.IP.",
"Device.Routing.",
"Device.DNS.",
"Device.Firewall.",
"Device.NAT.",
"Device.DHCPv4.",
"Device.DHCPv6.",
"Device.Users.",
"Device.UPnP.",
"Device.X_CISCO_COM_DDNS.",
"Device.X_CISCO_COM_Security.",
"Device.X_CISCO_COM_DeviceControl.",
"Device.Bridging.",
"Device.RouterAdvertisement.",
"Device.NeighborDiscovery.",
"Device.IPv6rd.",
"Device.X_CISCO_COM_MLD.",
"Device.X_CISCO_COM_CableModem.",
"Device.X_Comcast_com_ParentalControl.",
"Device.X_CISCO_COM_Diagnostics.",
"Device.X_CISCO_COM_MultiLAN.",
"Device.X_COMCAST_COM_GRE.",
"Device.X_CISCO_COM_GRE.",
"Device.Hosts.",
"Device.ManagementServer."
};
 
static char *subObjectList[] = 
{
"Device.DeviceInfo.NetworkProperties.",
"Device.MoCA.X_CISCO_COM_WiFi_Extender.",
"Device.MoCA.Interface.",
"Device.IP.Diagnostics.",
"Device.IP.Interface.",
"Device.DNS.Diagnostics.",
"Device.DNS.Client.",
"Device.DeviceInfo.VendorConfigFile.",
"Device.DeviceInfo.MemoryStatus.",
"Device.DeviceInfo.ProcessStatus.",
"Device.DeviceInfo.Webpa.",
"Device.DeviceInfo.SupportedDataModel."
}; 

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static WAL_STATUS mapStatus(int ret);
static void ccspWebPaValueChangedCB(parameterSigStruct_t* val, int size,void* user_data);
static PARAMVAL_CHANGE_SOURCE mapWriteID(unsigned int writeID);
static int getParamValues(char *pParameterName, ParamVal ***parametervalArr,int *TotalParams);
static int getAtomicParamValues(char *pParameterName[], int paramCount, char *CompName, char *dbusPath, ParamVal ***parametervalArr, int startIndex);
static void free_ParamCompList(ParamCompList *ParamGroup, int compCount);
static int getParamAttributes(char *pParameterName, AttrVal ***attr, int *TotalParams);
static int setParamValues(ParamVal paramVal[], int paramCount, const WEBPA_SET_TYPE setType);
static int setParamAttributes(const char *pParameterName, const AttrVal *attArr);
static int prepare_parameterValueStruct(parameterValStruct_t* val, ParamVal *paramVal, char *paramName);
static void free_set_param_values_memory(parameterValStruct_t* val, int paramCount, char * faultParam);
static void identifyRadioIndexToReset(int paramCount, parameterValStruct_t* val,BOOL *bRestartRadio1,BOOL *bRestartRadio2);
static int IndexMpa_WEBPAtoCPE(char *pParameterName);
static void IndexMpa_CPEtoWEBPA(char **ppParameterName);
static int getMatchingComponentValArrayIndex(char *objectName);
static int getMatchingSubComponentValArrayIndex(char *objectName);
static void getObjectName(char *str, char *objectName, int objectLevel);
static int getComponentInfoFromCache(char *parameterName, char *objectName, char *compName, char *dbusPath);
static void initApplyWiFiSettings();
static void *applyWiFiSettingsTask();

extern void getCurrentTime(struct timeval *timer);
extern long timeValDiff(struct timeval *starttime, struct timeval *finishtime);
extern ANSC_HANDLE bus_handle;

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
/**
 * @brief Registers the notification callback function.
 *
 * @param[in] cb Notification callback function.
 * @return WAL_STATUS.
 */
WAL_STATUS RegisterNotifyCB(notifyCB cb)
{
	notifyCbFn = cb;
	return WAL_SUCCESS;
}

/**
 * @brief getValues Returns the parameter Names from stack for GET request
 *
 * @param[in] paramName parameter Name
 * @param[in] paramCount Number of parameters
 * @param[in] paramValArr parameter value Array
 * @param[out] retValCount Number of parameters returned from stack
 * @param[out] retStatus Returns parameter Value from the stack
 */
void getValues(const char *paramName[], const unsigned int paramCount, ParamVal ***paramValArr,
		int *retValCount, WAL_STATUS *retStatus)
{
	int cnt1=0, cnt2=0, compCount=0, matchFlag=0, subParamCount=0, ret = -1, startIndex = 0,size = 0, error = 0, retIndex=0, index = -1;
	char parameterName[MAX_PARAMETERNAME_LEN] = {'\0'};
	char objectName[MAX_PARAMETERNAME_LEN] = {'\0'};
	ParamCompList *ParamGroup = NULL;
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	char compName[MAX_PARAMETERNAME_LEN/2] = { 0 };
	char dbusPath[MAX_PARAMETERNAME_LEN/2] = { 0 };
	componentStruct_t ** ppComponents = NULL;	
	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr),"%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);
	
	for(cnt1 = 0; cnt1 < paramCount; cnt1++)
	{
		matchFlag = 0;

		// Get the matching component index from cache
		walStrncpy(parameterName,paramName[cnt1],sizeof(parameterName));
		index = getComponentInfoFromCache(parameterName, objectName, compName, dbusPath);         	
		
		// Cannot identify the component from cache, make DBUS call to fetch component
		if(index == -1 || ComponentValArray[index].comp_size > 2) //anything above size > 2
		{
			WalPrint("in if for size >2\n");
			// GET Component for parameter from stack
			WalPrint("ComponentValArray[index].comp_size : %d\n",ComponentValArray[index].comp_size);
			walStrncpy(parameterName,paramName[cnt1],sizeof(parameterName));
			retIndex = IndexMpa_WEBPAtoCPE(parameterName);
			if(retIndex == -1)
			{
				ret = CCSP_ERR_INVALID_PARAMETER_NAME;
				WalError("Parameter name is not supported, invalid index. ret = %d\n", ret);
				error = 1;
				break;
			}
			WalPrint("Get component for parameterName : %s from stack\n",parameterName);

			ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
		        dst_pathname_cr, parameterName, l_Subsystem, &ppComponents, &size);
			WalPrint("size : %d, ret : %d\n",size,ret);
			
			if (ret == CCSP_SUCCESS && size == 1)
			{	
				walStrncpy(compName,ppComponents[0]->componentName,sizeof(compName));
				walStrncpy(dbusPath,ppComponents[0]->dbusPath,sizeof(dbusPath));
				free_componentStruct_t(bus_handle, size, ppComponents);
			}
			else
			{
				WalError("Parameter name %s is not supported. ret = %d\n", parameterName, ret);
				free_componentStruct_t(bus_handle, size, ppComponents);
				error = 1;
				break;
			}
		}
	
		WalPrint("parameterName: %s, compName : %s, dbusPath : %s\n", parameterName, compName, dbusPath);
		if(ParamGroup == NULL)
		{
			WalPrint("ParamCompList is null initializing\n");				
			ParamGroup = (ParamCompList *) malloc(sizeof(ParamCompList));
		   	ParamGroup[0].parameterCount = 1;
		   	ParamGroup[0].comp_name = (char *) malloc(MAX_PARAMETERNAME_LEN/2);
			walStrncpy(ParamGroup[0].comp_name, compName,MAX_PARAMETERNAME_LEN/2);
		   	ParamGroup[0].dbus_path = (char *) malloc(MAX_PARAMETERNAME_LEN/2);
			walStrncpy(ParamGroup[0].dbus_path, dbusPath,MAX_PARAMETERNAME_LEN/2);

			// max number of parameter will be equal to the remaining parameters to be iterated (i.e. paramCount - cnt1)
			ParamGroup[0].parameterName = (char **) malloc(sizeof(char *) * (paramCount - cnt1));			   
		   	ParamGroup[0].parameterName[0] = (char *) malloc(MAX_PARAMETERNAME_LEN);
		   	walStrncpy(ParamGroup[0].parameterName[0],paramName[cnt1],MAX_PARAMETERNAME_LEN);

		   	compCount++;
		}
		else
		{
		   	WalPrint("ParamCompList exists checking if parameter belongs to existing group\n");

			for(cnt2 = 0; cnt2 < compCount; cnt2++)
			{
				if(!strcmp(ParamGroup[cnt2].comp_name,compName))
	      		{
					WalPrint("Match found to already existing component group in ParamCompList, adding parameter to it\n");
					ParamGroup[cnt2].parameterCount = ParamGroup[cnt2].parameterCount + 1;
					subParamCount =  ParamGroup[cnt2].parameterCount;
					WalPrint("subParamCount :%d\n",subParamCount);
				
					ParamGroup[cnt2].parameterName[subParamCount-1] = (char *) malloc(MAX_PARAMETERNAME_LEN);
						
					walStrncpy(ParamGroup[cnt2].parameterName[subParamCount-1],paramName[cnt1],MAX_PARAMETERNAME_LEN);
					WalPrint("ParamGroup[%d].parameterName :%s\n",cnt2,ParamGroup[cnt2].parameterName[subParamCount-1]);
						
					matchFlag=1;
					break;
	      		}
	    	}
	    	if(matchFlag != 1)
	    	{
			WalPrint("Parameter does not belong to existing component group, creating new group \n");
				    
		      	ParamGroup =  (ParamCompList *) realloc(ParamGroup,sizeof(ParamCompList) * (compCount + 1));
		      	ParamGroup[compCount].parameterCount = 1;
		      	ParamGroup[compCount].comp_name = (char *) malloc(MAX_PARAMETERNAME_LEN/2);
				walStrncpy(ParamGroup[compCount].comp_name, compName,MAX_PARAMETERNAME_LEN/2);
		      	ParamGroup[compCount].dbus_path = (char *) malloc(MAX_PARAMETERNAME_LEN/2);
				walStrncpy(ParamGroup[compCount].dbus_path, dbusPath,MAX_PARAMETERNAME_LEN/2);

			// max number of parameter will be equal to the remaining parameters to be iterated (i.e. paramCount - cnt1)	  
		      	ParamGroup[compCount].parameterName = (char **) malloc(sizeof(char *) * (paramCount - cnt1));
		        ParamGroup[compCount].parameterName[0] = (char *) malloc(MAX_PARAMETERNAME_LEN);
		      	walStrncpy(ParamGroup[compCount].parameterName[0],paramName[cnt1],MAX_PARAMETERNAME_LEN);
				      
		       	WalPrint("ParamGroup[%d]->comp_name :%s\n",compCount,ParamGroup[compCount].comp_name);
		      	WalPrint("ParamGroup[%d].parameterName :%s\n",compCount,ParamGroup[compCount].parameterName[0]);
				      
		      	compCount++;			     
	    	}		    
	 	}
	}//End of for loop
	   
	WalPrint("Number of parameter groups : %d\n",compCount);
	
	if(error != 1)
	{
		for(cnt1 = 0; cnt1 < compCount; cnt1++)
		{
			WalInfo("********** Parameter group ****************\n");
		  	WalInfo("ParamGroup[%d]->comp_name :%s, ParamGroup[%d]->dbus_path :%s, ParamGroup[%d]->parameterCount :%d\n",cnt1,ParamGroup[cnt1].comp_name, cnt1,ParamGroup[cnt1].dbus_path, cnt1,ParamGroup[cnt1].parameterCount);
		  	
		  	for(cnt2 = 0; cnt2 < ParamGroup[cnt1].parameterCount; cnt2++)
		  	{
			 		WalInfo("ParamGroup[%d].parameterName :%s\n",cnt1,ParamGroup[cnt1].parameterName[cnt2]);
		  	}
			if(!strcmp(ParamGroup[cnt1].comp_name,wifiCompName)) 
			{
				WalPrint("Before mutex lock in getValues\n");
				pthread_mutex_lock (&applySetting_mutex);
				WalPrint("After mutex lock in getValues\n");
			}
		  	// GET atomic value call
			WalPrint("startIndex %d\n",startIndex);
		  	ret = getAtomicParamValues(ParamGroup[cnt1].parameterName, ParamGroup[cnt1].parameterCount, ParamGroup[cnt1].comp_name, ParamGroup[cnt1].dbus_path, paramValArr, startIndex);
			
			if(!strcmp(ParamGroup[cnt1].comp_name,wifiCompName)) 
			{
				pthread_mutex_unlock (&applySetting_mutex);
				WalPrint("After thread unlock in getValues\n");
			}
		  	if(ret != CCSP_SUCCESS)
		  	{
				WalError("Get Atomic Values call failed for ParamGroup[%d]->comp_name :%s ret: %d\n",cnt1,ParamGroup[cnt1].comp_name,ret);
				break;
		  	}
		
			startIndex = startIndex + ParamGroup[cnt1].parameterCount;
		}
	}
	
	for (cnt1 = 0; cnt1 < paramCount; cnt1++)
	{
		retStatus[cnt1] = mapStatus(ret);
		retValCount[cnt1] = 1;	
	}
	
	free_ParamCompList(ParamGroup, compCount);
}

/**
 * @brief To free allocated memory for ParamCompList
 *
 * @param[in] ParamGroup ParamCompList formed during GET request to group parameters based on components
 * @param[in] compCount number of components  
 */
static void free_ParamCompList(ParamCompList *ParamGroup, int compCount)
{
	int cnt1 = 0, cnt2 = 0;
	for(cnt1 = 0; cnt1 < compCount; cnt1++)
	{
	  	for(cnt2 = 0; cnt2 < ParamGroup[cnt1].parameterCount; cnt2++)
	  	{
	     		WAL_FREE(ParamGroup[cnt1].parameterName[cnt2]);
	  	}
		WAL_FREE(ParamGroup[cnt1].parameterName);
		WAL_FREE(ParamGroup[cnt1].comp_name);
		WAL_FREE(ParamGroup[cnt1].dbus_path);
	}
	WAL_FREE(ParamGroup);
}

static int getComponentInfoFromCache(char *parameterName, char *objectName, char *compName, char *dbusPath)
{   
	int index = -1;	
	
	getObjectName(parameterName, objectName, 1);
	index = getMatchingComponentValArrayIndex(objectName);
	WalPrint("objectLevel: 1, parameterName: %s, objectName: %s, matching index: %d\n",parameterName,objectName,index);
		 
	if((index != -1) && (ComponentValArray[index].comp_size == 1))
	{
		strcpy(compName,ComponentValArray[index].comp_name);
		strcpy(dbusPath,ComponentValArray[index].dbus_path);
	}
	else if((index != -1) && (ComponentValArray[index].comp_size == 2))
	{
		getObjectName(parameterName, objectName, 2);
		index = getMatchingSubComponentValArrayIndex(objectName);
		WalPrint("objectLevel: 2, parameterName: %s, objectName: %s, matching index=%d\n",parameterName,objectName,index);
		if(index != -1 )
		{
			strcpy(compName,SubComponentValArray[index].comp_name);
			strcpy(dbusPath,SubComponentValArray[index].dbus_path); 
		}
    	}
	return index;	
}

/**
 * @brief getAttributes Returns the parameter Value Attributes from stack for GET request
 *
 * @param[in] paramName parameter Name
 * @param[in] paramCount Number of parameters
 * @param[in] paramValArr parameter value Array
 * @param[out] retValCount Number of parameters returned from stack
 * @param[out] retStatus Returns parameter Value from the stack
 */
void getAttributes(const char *paramName[], const unsigned int paramCount, AttrVal ***attr, int *retAttrCount, WAL_STATUS *retStatus)
{
	int cnt = 0;
	int ret = 0;

	for (cnt = 0; cnt < paramCount; cnt++)
	{
		ret = getParamAttributes(paramName[cnt], &attr[cnt], &retAttrCount[cnt]);
		retStatus[cnt] = mapStatus(ret);
		WalPrint("Parameter Name: %s, Parameter Attributes return: %d\n",paramName[cnt],retStatus[cnt]);
	}
}

/**
 * @brief setValues Sets the parameter value
 *
 * @param[in] paramName parameter Name
 * @param[in] paramCount Number of parameters
 * @param[in] setType Set operation type
 * @param[out] retStatus Returns parameter Value from the stack
 */

void setValues(const ParamVal paramVal[], const unsigned int paramCount, const WEBPA_SET_TYPE setType, WAL_STATUS *retStatus)
{
	int cnt = 0, ret = 0;
	ret = setParamValues(paramVal, paramCount, setType);

	for (cnt = 0; cnt < paramCount; cnt++) 
	{
		retStatus[cnt] = mapStatus(ret);
	}
}

/**
 * @brief setAttributes Returns the status of parameter from stack for SET request
 *
 * @param[in] paramName parameter Name
 * @param[in] paramCount Number of parameters
 * @param[in] attArr parameter value Array
 * @param[out] retStatus Returns parameter Value from the stack
 */
void setAttributes(const char *paramName[], const unsigned int paramCount,
		const AttrVal *attArr[], WAL_STATUS *retStatus)
{
	int cnt = 0, ret = 0;

	for (cnt = 0; cnt < paramCount; cnt++)
	{
		ret = setParamAttributes(paramName[cnt], attArr[cnt]);
		retStatus[cnt] = mapStatus(ret);
	}

}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/*
* @brief WAL_STATUS mapStatus Defines WAL status values from corresponding ccsp values
* @param[in] ret ccsp status values from stack
*/
static WAL_STATUS mapStatus(int ret)
{
	switch (ret) {
		case CCSP_SUCCESS:
			return WAL_SUCCESS;
		case CCSP_FAILURE:
			return WAL_FAILURE;
		case CCSP_ERR_TIMEOUT:
			return WAL_ERR_TIMEOUT;
		case CCSP_ERR_NOT_EXIST:
			return WAL_ERR_NOT_EXIST;
		case CCSP_ERR_INVALID_PARAMETER_NAME:
			return WAL_ERR_INVALID_PARAMETER_NAME;
		case CCSP_ERR_INVALID_PARAMETER_TYPE:
			return WAL_ERR_INVALID_PARAMETER_TYPE;
		case CCSP_ERR_INVALID_PARAMETER_VALUE:
			return WAL_ERR_INVALID_PARAMETER_VALUE;
		case CCSP_ERR_NOT_WRITABLE:
			return WAL_ERR_NOT_WRITABLE;
		case CCSP_ERR_SETATTRIBUTE_REJECTED:
			return WAL_ERR_SETATTRIBUTE_REJECTED;
		case CCSP_CR_ERR_NAMESPACE_OVERLAP:
			return WAL_ERR_NAMESPACE_OVERLAP;
		case CCSP_CR_ERR_UNKNOWN_COMPONENT:
			return WAL_ERR_UNKNOWN_COMPONENT;
		case CCSP_CR_ERR_NAMESPACE_MISMATCH:
			return WAL_ERR_NAMESPACE_MISMATCH;
		case CCSP_CR_ERR_UNSUPPORTED_NAMESPACE:
			return WAL_ERR_UNSUPPORTED_NAMESPACE;
		case CCSP_CR_ERR_DP_COMPONENT_VERSION_MISMATCH:
			return WAL_ERR_DP_COMPONENT_VERSION_MISMATCH;
		case CCSP_CR_ERR_INVALID_PARAM:
			return WAL_ERR_INVALID_PARAM;
		case CCSP_CR_ERR_UNSUPPORTED_DATATYPE:
			return WAL_ERR_UNSUPPORTED_DATATYPE;
		default:
			return WAL_FAILURE;
	}
}
/**
 * @brief ccspWebPaValueChangedCB callback function for set notification
 *
 * @param[in] val parameterSigStruct_t notification struct got from stack
 * @param[in] size 
 * @param[in] user_data
 */
static void ccspWebPaValueChangedCB(parameterSigStruct_t* val, int size, void* user_data)
{
	WalPrint("Inside CcspWebpaValueChangedCB\n");

	ParamNotify *paramNotify;

	if (NULL == notifyCbFn) {
		WalError("Fatal: ccspWebPaValueChangedCB() notifyCbFn is NULL\n");
		return;
	}

	paramNotify= (ParamNotify *) malloc(sizeof(ParamNotify));
	paramNotify->paramName = val->parameterName;
	paramNotify->oldValue= val->oldValue;
	paramNotify->newValue = val->newValue;
	paramNotify->type = val->type;
	paramNotify->changeSource = mapWriteID(val->writeID);

	WalInfo("Notification Event from stack: Parameter Name: %s, Old Value: %s, New Value: %s, Data Type: %d, Change Source: %d\n",
			paramNotify->paramName, paramNotify->oldValue, paramNotify->newValue, paramNotify->type, paramNotify->changeSource);

	(*notifyCbFn)(paramNotify);
}

/**
 * @brief sendIoTNotification function to send IoT notification
 *
 * @param[in] iotMsg IoT notification message
 * @param[in] size Size of IoT notification message
 */
void sendIoTNotification(void* iotMsg, int size)
{
	char* str = NULL;

	ParamNotify *paramNotify;

	if (NULL == notifyCbFn) {
		WalError("Fatal: sendIoTNotification() notifyCbFn is NULL\n");
		return;
	}

	paramNotify = (ParamNotify *) malloc(sizeof(ParamNotify));
	paramNotify->paramName = (char *) malloc(8);
	walStrncpy(paramNotify->paramName, "IOT", 8);
	paramNotify->oldValue = NULL;

	if((str = (char*) malloc(size+1)) == NULL)
	{
		WalError("Error allocating memory in sendIoTNotification fn\n");
		WAL_FREE(iotMsg);
		return;
	}
	sprintf(str, "%.*s", size, (char *)iotMsg);
	//WAL_FREE(iotMsg); //TODO: Free memory?
	paramNotify->newValue = str;
	paramNotify->type = WAL_STRING;
	paramNotify->changeSource = CHANGED_BY_UNKNOWN;

	WalInfo("Notification Event from IoT: Parameter Name: %s, New Value: %s, Data Type: %d, Change Source: %d\n",
			paramNotify->paramName, paramNotify->newValue, paramNotify->type, paramNotify->changeSource);

	(*notifyCbFn)(paramNotify);
}

static PARAMVAL_CHANGE_SOURCE mapWriteID(unsigned int writeID)
{
	PARAMVAL_CHANGE_SOURCE source;
	WalPrint("Inside mapWriteID\n");
	WalPrint("WRITE ID is %d\n", writeID);

	switch(writeID)
	{
		case CCSP_COMPONENT_ID_ACS:
			source = CHANGED_BY_ACS;
			break;
		case CCSP_COMPONENT_ID_WebPA:
			source = CHANGED_BY_WEBPA;
			break;
		case CCSP_COMPONENT_ID_XPC:
			source = CHANGED_BY_XPC;
			break;
		case DSLH_MPA_ACCESS_CONTROL_CLIENTTOOL:
			source = CHANGED_BY_CLI;
			break;
		case CCSP_COMPONENT_ID_SNMP:
			source = CHANGED_BY_SNMP;
			break;
		case CCSP_COMPONENT_ID_WebUI:
			source = CHANGED_BY_WEBUI;
			break;
		default:
			source = CHANGED_BY_UNKNOWN;
			break;
	}

	WalPrint("CMC/component_writeID is: %d\n", source);
	return source;
}

/**
 * @brief getParamValues Returns the parameter Values from stack for GET request
 *
 * @param[in] paramName parameter Name
 * @param[in] paramValArr parameter value Array
 * @param[out] TotalParams Number of parameters returned from stack
 */
static int getParamValues(char *pParameterName, ParamVal ***parametervalArr, int *TotalParams)
{
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	int ret = 0, i = 0, size = 0, val_size = 0;
	char *parameterNames[1];
	char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
	char *p = &paramName;
	
	componentStruct_t ** ppComponents = NULL;
	parameterValStruct_t **parameterval = NULL;
	parameterValStruct_t *parametervalError = NULL;
	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	walStrncpy(paramName, pParameterName,sizeof(paramName));
	snprintf(dst_pathname_cr,sizeof(dst_pathname_cr), "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);
	IndexMpa_WEBPAtoCPE(paramName);
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem, &ppComponents, &size);

	if (ret == CCSP_SUCCESS && size == 1)
	{
		parameterNames[0] = p;
		ret = CcspBaseIf_getParameterValues(bus_handle,
				ppComponents[0]->componentName, ppComponents[0]->dbusPath,
				parameterNames,
				1, &val_size, &parameterval);

		if (ret != CCSP_SUCCESS)
		{
			parametervalArr[0] = (ParamVal **) malloc(sizeof(ParamVal *) * 1);
			parametervalError = (parameterValStruct_t *) malloc(sizeof(parameterValStruct_t));
			parametervalError->parameterValue = NULL;
			parametervalError->parameterName = (char *)malloc(sizeof(char)*MAX_PARAMETERNAME_LEN);
			walStrncpy(parametervalError->parameterName,pParameterName,MAX_PARAMETERNAME_LEN);
			parametervalError->type = ccsp_string;
			parametervalArr[0][0] = parametervalError;
			*TotalParams = 1;
			WalError("Failed to GetValue for param: %s ret: %d\n", paramName, ret);

		}
		else
		{
			*TotalParams = val_size;
			parametervalArr[0] = (ParamVal **) malloc(sizeof(ParamVal*) * val_size);
			for (i = 0; i < val_size; i++)
			{
				IndexMpa_CPEtoWEBPA(&parameterval[i]->parameterName);
				parametervalArr[0][i] = parameterval[i];
				WalPrint("success: %s %s %d \n",parametervalArr[0][i]->name,parametervalArr[0][i]->value,parametervalArr[0][i]->type);
			}
			WAL_FREE(parameterval);
		}
	}
	else
	{
		parametervalArr[0] = (ParamVal **) malloc(sizeof(ParamVal *) * 1);
		parametervalError = (parameterValStruct_t *) malloc(sizeof(parameterValStruct_t));
		parametervalError->parameterValue = NULL;
		parametervalError->parameterName = (char *)malloc(sizeof(char)*MAX_PARAMETERNAME_LEN);
		walStrncpy(parametervalError->parameterName,pParameterName,MAX_PARAMETERNAME_LEN);
		parametervalError->type = ccsp_string;
		parametervalArr[0][0] = parametervalError;
		*TotalParams = 1;
		WalError("Parameter name is not supported.ret : %d\n", ret);
	}
	free_componentStruct_t(bus_handle, size, ppComponents);
	return ret;
}


/**
 * @brief getAtomicParamValues Returns the parameter Values from stack for GET request in atomic way.
 * This is optimized as it fetches component from pre-populated ComponentValArray and does bulk GET 
 * for parameters belonging to same component
 *
 * @param[in] parameterNames parameter Names List
 * @param[in] paramCount number of parameters
 * @param[in] CompName Component Name of parameters
 * @param[in] dbusPath Dbus Path of component
 * @param[out] paramValArr parameter value Array
 * @param[in] startIndex starting array index to fill the output paramValArr
 */
static int getAtomicParamValues(char *parameterNames[], int paramCount, char *CompName, char *dbusPath, ParamVal ***parametervalArr, int startIndex)
{
	int ret = 0, val_size = 0, cnt=0, retIndex=0, error=0;
	char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
	char **parameterNamesLocal = NULL;
	parameterValStruct_t **parameterval = NULL;

	WalPrint(" ------ Start of getAtomicParamValues ----\n");
	parameterNamesLocal = (char **) malloc(sizeof(char *) * paramCount);
	memset(parameterNamesLocal,0,(sizeof(char *) * paramCount));

	// Initialize names array with converted index	
	for (cnt = 0; cnt < paramCount; cnt++)
	{
		WalPrint("Before parameterNames[%d] : %s\n",cnt,parameterNames[cnt]);
	
		parameterNamesLocal[cnt] = (char *) malloc(sizeof(char) * (strlen(parameterNames[cnt]) + 1));
		strcpy(parameterNamesLocal[cnt],parameterNames[cnt]);

		retIndex=IndexMpa_WEBPAtoCPE(parameterNamesLocal[cnt]);
		if(retIndex == -1)
		{
		 	ret = CCSP_ERR_INVALID_PARAMETER_NAME;
		 	WalError("Parameter name is not supported, invalid index. ret = %d\n", ret);
			error = 1;
			break;
		}

		WalPrint("After parameterNamesLocal[%d] : %s\n",cnt,parameterNamesLocal[cnt]);
	}
	
	if(error != 1)
	{
		WalPrint("CompName = %s, dbusPath : %s, paramCount = %d\n", CompName, dbusPath, paramCount);
	 
		ret = CcspBaseIf_getParameterValues(bus_handle,CompName,dbusPath,parameterNamesLocal,paramCount, &val_size, &parameterval);
		WalPrint("----- After GPA ret = %d------\n",ret);
		if (ret != CCSP_SUCCESS)
		{
			WalError("Error:Failed to GetValue for parameters ret: %d\n", ret);
		}
		else
		{
			WalPrint("val_size : %d\n",val_size);
			if(startIndex == 0)
			{
				parametervalArr[0] = (ParamVal **) malloc(sizeof(ParamVal*) * val_size);
			}
			else
			{
				parametervalArr[0] = (ParamVal **) realloc(parametervalArr[0],sizeof(ParamVal*) * (startIndex + val_size));
			}
			for (cnt = 0; cnt < val_size; cnt++)
			{
				WalPrint("cnt+startIndex : %d\n",cnt+startIndex);
				IndexMpa_CPEtoWEBPA(&parameterval[cnt]->parameterName);
				parametervalArr[0][cnt+startIndex] = parameterval[cnt];
				WalPrint("success: %s %s %d \n",parametervalArr[0][cnt+startIndex]->name,parametervalArr[0][cnt+startIndex]->value,parametervalArr[0][cnt+startIndex]->type);
			}

			WAL_FREE(parameterval);
		}	
	}
		
	for (cnt = 0; cnt < paramCount; cnt++)
	{
		WAL_FREE(parameterNamesLocal[cnt]);
	}
	WAL_FREE(parameterNamesLocal);
	return ret;
}


/**
 * @brief getParamAttributes Returns the parameter Values from stack for GET request
 *
 * @param[in] paramName parameter Name
 * @param[in] attr parameter value Array
 * @param[out] TotalParams Number of parameters returned from stack
 */
 
static int getParamAttributes(char *pParameterName, AttrVal ***attr, int *TotalParams)
{
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	int size = 0, ret = 0, sizeAttrArr = 0, x = 0;
	char *parameterNames[1];
	componentStruct_t ** ppComponents = NULL;
	char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
	char *p = &paramName;
	
	parameterAttributeStruct_t** ppAttrArray = NULL;
	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	walStrncpy(paramName, pParameterName,sizeof(paramName));
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr), "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);

	IndexMpa_WEBPAtoCPE(paramName);
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem,  //prefix
			&ppComponents, &size);

	if (ret == CCSP_SUCCESS && size == 1)
	{
		parameterNames[0] = p;

		ret = CcspBaseIf_getParameterAttributes(bus_handle,
				ppComponents[0]->componentName, ppComponents[0]->dbusPath,
				parameterNames, 1, &sizeAttrArr, &ppAttrArray);

		if (CCSP_SUCCESS != ret)
		{
			attr[0] = (AttrVal **) malloc(sizeof(AttrVal *) * 1);
			attr[0][0] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
			attr[0][0]->name = (char *) malloc(sizeof(char) * MAX_PARAMETERNAME_LEN);
			attr[0][0]->value = (char *) malloc(sizeof(char) * MAX_PARAMETERVALUE_LEN);
			sprintf(attr[0][0]->value, "%d", -1);
			walStrncpy(attr[0][0]->name, pParameterName,MAX_PARAMETERNAME_LEN);
			attr[0][0]->type = WAL_INT;
			*TotalParams = 1;
			WalError("Failed to GetValue for GetParamAttr ret : %d \n", ret);
		}
		else
		{
			*TotalParams = sizeAttrArr;
			WalPrint("sizeAttrArr: %d\n",sizeAttrArr);
			attr[0] = (AttrVal **) malloc(sizeof(AttrVal *) * sizeAttrArr);
			for (x = 0; x < sizeAttrArr; x++)
			{
				attr[0][x] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
				attr[0][x]->name = (char *) malloc(sizeof(char) * MAX_PARAMETERNAME_LEN);
				attr[0][x]->value = (char *) malloc(sizeof(char) * MAX_PARAMETERVALUE_LEN);

				IndexMpa_CPEtoWEBPA(&ppAttrArray[x]->parameterName);
				walStrncpy(attr[0][x]->name, ppAttrArray[x]->parameterName,MAX_PARAMETERNAME_LEN);
				sprintf(attr[0][x]->value, "%d", ppAttrArray[x]->notification);
				attr[0][x]->type = WAL_INT;
			}
		}
		free_parameterAttributeStruct_t(bus_handle, sizeAttrArr, ppAttrArray);
		free_componentStruct_t(bus_handle, size, ppComponents);
	}
	else
	{
		attr[0] = (AttrVal **) malloc(sizeof(AttrVal *) * 1);
		attr[0][0] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
		attr[0][0]->name = (char *) malloc(sizeof(char) * MAX_PARAMETERNAME_LEN);
		attr[0][0]->value = (char *) malloc(sizeof(char) * MAX_PARAMETERVALUE_LEN);
		sprintf(attr[0][0]->value, "%d", -1);
		walStrncpy(attr[0][0]->name, pParameterName,MAX_PARAMETERNAME_LEN);
		attr[0][0]->type = WAL_INT;
		*TotalParams = 1;
		WalError("Parameter name is not supported.ret : %d\n", ret);
	}
	return ret;
}

/**
 * @brief setParamValues Returns the status from stack for SET request
 *
 * @param[in] paramVal parameter value Array
 * @param[in] paramCount Number of parameters
 * @param[in] setType set for atomic set
 */

static int setParamValues(ParamVal paramVal[], int paramCount, const WEBPA_SET_TYPE setType)
{
	char* faultParam = NULL;
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	int ret=0, size = 0, cnt = 0, cnt1=0, retIndex=0, index = -1;
	componentStruct_t ** ppComponents = NULL;
	char CompName[MAX_PARAMETERNAME_LEN/2] = { 0 };
	char tempCompName[MAX_PARAMETERNAME_LEN/2] = { 0 };
	char dbusPath[MAX_PARAMETERNAME_LEN/2] = { 0 };
	char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
	char objectName[MAX_PARAMETERNAME_LEN] = { 0 };
	unsigned int writeID = CCSP_COMPONENT_ID_WebPA;
	
	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr), "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);

	parameterValStruct_t* val = (parameterValStruct_t*) malloc(sizeof(parameterValStruct_t) * paramCount);
	memset(val,0,(sizeof(parameterValStruct_t) * paramCount));
	
	walStrncpy(paramName, paramVal[0].name,sizeof(paramName));
	retIndex = IndexMpa_WEBPAtoCPE(paramName);
	if(retIndex== -1)
	{
		ret = CCSP_ERR_INVALID_PARAMETER_NAME;
		WalError("Parameter name %s is not supported.ret : %d\n", paramName, ret);
		WAL_FREE(val);
		return ret;
	}
	
	index = getComponentInfoFromCache(paramName,objectName,CompName,dbusPath);
	
	// Cannot identify the component from cache, make DBUS call to fetch component
	if(index == -1 || ComponentValArray[index].comp_size > 2) //anything above size > 2
	{
		WalPrint("in if for size >2\n");
		// GET Component for parameter from stack
		WalPrint("ComponentValArray[index].comp_size : %d\n",ComponentValArray[index].comp_size);
		walStrncpy(paramName,paramVal[0].name,sizeof(paramName));
		retIndex = IndexMpa_WEBPAtoCPE(paramName);
		if(retIndex == -1)
		{
			ret = CCSP_ERR_INVALID_PARAMETER_NAME;
			WalError("Parameter name is not supported, invalid index. ret = %d\n", ret);
			WAL_FREE(val);
			return ret;
		}
		WalPrint("Get component for paramName : %s from stack\n",paramName);

		ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem, &ppComponents, &size);
		WalPrint("size : %d, ret : %d\n",size,ret);
		
		if (ret == CCSP_SUCCESS && size == 1)
		{	
			walStrncpy(CompName,ppComponents[0]->componentName,sizeof(CompName));
			walStrncpy(dbusPath,ppComponents[0]->dbusPath,sizeof(dbusPath));
			free_componentStruct_t(bus_handle, size, ppComponents);
		}
		else
		{
			WalError("Parameter name %s is not supported. ret = %d\n", paramName, ret);
			free_componentStruct_t(bus_handle, size, ppComponents);
			WAL_FREE(val);
			return ret;
		}
	}
	
	WalInfo("parameterName: %s, CompName : %s, dbusPath : %s\n", paramName, CompName, dbusPath);

	for (cnt = 0; cnt < paramCount; cnt++) 
	{
	    	retIndex=0;
		walStrncpy(paramName, paramVal[cnt].name,sizeof(paramName));
		retIndex = IndexMpa_WEBPAtoCPE(paramName);
		if(retIndex == -1)
		{
			ret = CCSP_ERR_INVALID_PARAMETER_NAME;
			WalError("Parameter name %s is not supported.ret : %d\n", paramName, ret);
			free_set_param_values_memory(val,paramCount,faultParam);
			return ret;
		}
		
		WalPrint("-------Starting parameter component comparison-----------\n");
		index = getComponentInfoFromCache(paramName,objectName,tempCompName,dbusPath);
				
		// Cannot identify the component from cache, make DBUS call to fetch component
		if(index == -1 || ComponentValArray[index].comp_size > 2) //anything above size > 2
		{
			WalPrint("in if for size >2\n");
			// GET Component for parameter from stack
			WalPrint("ComponentValArray[index].comp_size : %d\n",ComponentValArray[index].comp_size);
			walStrncpy(paramName,paramVal[cnt].name,sizeof(paramName));
			retIndex = IndexMpa_WEBPAtoCPE(paramName);
			if(retIndex == -1)
			{
				ret = CCSP_ERR_INVALID_PARAMETER_NAME;
				WalError("Parameter name is not supported, invalid index. ret = %d\n", ret);
				WAL_FREE(val);
				return ret;
			}
			WalPrint("Get component for paramName : %s from stack\n",paramName);

			ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
		        dst_pathname_cr, paramName, l_Subsystem, &ppComponents, &size);
			WalPrint("size : %d, ret : %d\n",size,ret);
			
			if (ret == CCSP_SUCCESS && size == 1)
			{	
				walStrncpy(tempCompName,ppComponents[0]->componentName,sizeof(tempCompName));
				free_componentStruct_t(bus_handle, size, ppComponents);
			}
			else
			{
				WalError("Parameter name %s is not supported. ret = %d\n", paramName, ret);
				free_componentStruct_t(bus_handle, size, ppComponents);
				WAL_FREE(val);
				return ret;
			}
		}
		
		WalInfo("parameterName: %s, tempCompName : %s\n", paramName, tempCompName);
		if (strcmp(CompName, tempCompName) != 0)
		{
			WalError("Error: Parameters does not belong to the same component\n");
			WAL_FREE(val);
			return CCSP_FAILURE;
		}
		WalPrint("--------- End of parameter component comparison------\n");
				
		ret = prepare_parameterValueStruct(&val[cnt], &paramVal[cnt], paramName);
		if(ret)
		{
			WalError("Preparing parameter value struct is Failed \n");
			free_set_param_values_memory(val,paramCount,faultParam);
			return ret;
		}
	}
		
	writeID = (setType == WEBPA_ATOMIC_SET_XPC)? CCSP_COMPONENT_ID_XPC: CCSP_COMPONENT_ID_WebPA;
	
	if(!strcmp(CompName,wifiCompName)) 
	{		
		identifyRadioIndexToReset(paramCount,val,&bRestartRadio1,&bRestartRadio2);
		WalPrint("Before mutex lock in setParamValues\n");
		pthread_mutex_lock (&applySetting_mutex);
		bRadioRestartEn = TRUE;		
	}
		
	ret = CcspBaseIf_setParameterValues(bus_handle, CompName, dbusPath, 0, writeID, val, paramCount, TRUE, &faultParam);
	if(!strcmp(CompName,wifiCompName)) 
	{
		if(ret == CCSP_SUCCESS) //signal apply settings thread only when set is success
		{
			pthread_cond_signal(&applySetting_cond);
			WalPrint("condition signalling in setParamValues\n");
		}
		
		pthread_mutex_unlock (&applySetting_mutex);
		WalPrint("mutex unlock in setParamValues\n");
		
	}	
	if (ret != CCSP_SUCCESS && faultParam) 
	{
		WalError("Failed to SetAtomicValue for param  '%s' ret : %d \n", faultParam, ret);
		free_set_param_values_memory(val,paramCount,faultParam);
		return ret;
	}
	free_set_param_values_memory(val,paramCount,faultParam);	
	return ret;
}

static void initApplyWiFiSettings()
{
	int err = 0;
	pthread_t applySettingsThreadId;
	WalPrint("============ initApplySettings ==============\n");
	err = pthread_create(&applySettingsThreadId, NULL, applyWiFiSettingsTask, NULL);
	if (err != 0) 
	{
		WalError("Error creating messages thread :[%s]\n", strerror(err));
	}
	else
	{
		WalPrint("applyWiFiSettings thread created Successfully\n");
	}
}

static void *applyWiFiSettingsTask()
{
	char CompName[MAX_PARAMETERNAME_LEN/2] = { 0 };
	char dbusPath[MAX_PARAMETERNAME_LEN/2] = { 0 };
	parameterValStruct_t *RadApplyParam = NULL;
	char* faultParam = NULL;
	unsigned int writeID = CCSP_COMPONENT_ID_WebPA;
	struct timeval start,end,*startPtr,*endPtr;
	startPtr = &start;
	endPtr = &end;
	int nreq = 0,ret=0;
	WalPrint("================= applyWiFiSettings ==========\n");
	parameterValStruct_t val_set[4] = { 
					{"Device.WiFi.Radio.1.X_CISCO_COM_ApplySettingSSID","1", ccsp_int},
					{"Device.WiFi.Radio.1.X_CISCO_COM_ApplySetting", "true", ccsp_boolean},
					{"Device.WiFi.Radio.2.X_CISCO_COM_ApplySettingSSID","2", ccsp_int},
					{"Device.WiFi.Radio.2.X_CISCO_COM_ApplySetting", "true", ccsp_boolean} };
	
	// Component cache index 0 maps to "Device.WiFi."
	if(ComponentValArray[0].comp_name != NULL && ComponentValArray[0].dbus_path != NULL)
	{				
		walStrncpy(CompName,ComponentValArray[0].comp_name,sizeof(CompName));
		walStrncpy(dbusPath,ComponentValArray[0].dbus_path,sizeof(dbusPath));
		WalPrint("CompName : %s dbusPath : %s\n",CompName,dbusPath);
	

		//Identify the radio and apply settings
		while(1)
		{
			WalPrint("Before cond wait in applyWiFiSettings\n");
			pthread_cond_wait(&applySetting_cond, &applySetting_mutex);
			getCurrentTime(startPtr);
			WalPrint("After cond wait in applyWiFiSettings\n");
			if(bRadioRestartEn)
			{
				bRadioRestartEn = FALSE;
			
				if((bRestartRadio1 == TRUE) && (bRestartRadio2 == TRUE)) 
				{
					WalPrint("Need to restart both the Radios\n");
					RadApplyParam = val_set;
					nreq = 4;
				}

				else if(bRestartRadio1) 
				{
					WalPrint("Need to restart Radio 1\n");
					RadApplyParam = val_set;
					nreq = 2;
				}
				else if(bRestartRadio2) 
				{
					WalPrint("Need to restart Radio 2\n");
					RadApplyParam = &val_set[2];
					nreq = 2;
				}
			
				// Reset radio flags
				bRestartRadio1 = FALSE;
				bRestartRadio2 = FALSE;
			
				WalPrint("nreq : %d writeID : %d\n",nreq,writeID);
				ret = CcspBaseIf_setParameterValues(bus_handle, CompName, dbusPath, 0, writeID, RadApplyParam, nreq, TRUE,&faultParam);
				WalInfo("After SPV in applyWiFiSettings ret = %d\n",ret);
				if (ret != CCSP_SUCCESS && faultParam) 
				{
					WalError("Failed to Set Apply Settings\n");
				}	
			}
			WalPrint("Before thread unlock in applyWiFiSettings\n");
			pthread_mutex_unlock (&applySetting_mutex);
			getCurrentTime(endPtr);
			WalInfo("Elapsed time for apply setting : %ld ms\n", timeValDiff(startPtr, endPtr));
		}
	}
	else
	{
		WalError("Failed to get WiFi component info from cache to initialize apply settings\n");
	}
	WalPrint("============ End =============\n");
}


/**
 * @brief free_set_param_values_memory to free memory allocated in setParamValues function
 *
 * @param[in] val parameter value Array
 * @param[in] paramCount parameter count
 * @param[in] faultParam fault Param
 */
static void free_set_param_values_memory(parameterValStruct_t* val, int paramCount, char * faultParam)
{
	WalPrint("Inside free_set_param_values_memory\n");	
	int cnt1 = 0;
	WAL_FREE(faultParam);

	for (cnt1 = 0; cnt1 < paramCount; cnt1++) 
	{
		WAL_FREE(val[cnt1].parameterName);
	}
	WAL_FREE(val);
}

/**
 * @brief prepare_parameterValueStruct returns parameter values
 *
 * @param[in] val parameter value Array
 * @param[in] paramVal parameter value Array
 * @param[in] paramName parameter name
 */
 
static int prepare_parameterValueStruct(parameterValStruct_t* val, ParamVal *paramVal, char *paramName)
{
	val->parameterName = malloc( sizeof(char) * MAX_PARAMETERNAME_LEN);

	if(val->parameterName == NULL)
	{
		return WAL_FAILURE;
	}
	strcpy(val->parameterName,paramName);

	val->parameterValue = paramVal->value;
		
	switch(paramVal->type)
	{ 
		case 0:
				val->type = ccsp_string;
				break;
		case 1:
				val->type = ccsp_int;
				break;
		case 2:
				val->type = ccsp_unsignedInt;
				break;
		case 3:
				val->type = ccsp_boolean;
				break;
		case 4:
				val->type = ccsp_dateTime;
				break;
		case 5:
				val->type = ccsp_base64;
				break;
		case 6:
				val->type = ccsp_long;
				break;
		case 7:
				val->type = ccsp_unsignedLong;
				break;
		case 8:
				val->type = ccsp_float;
				break;
		case 9:
				val->type = ccsp_double;
				break;
		case 10:
				val->type = ccsp_byte;
				break;
		default:
				val->type = ccsp_none;
				break;
	}
	return WAL_SUCCESS;
}

/**
 * @brief identifyRadioIndexToReset identifies which radio to restart 
 *
 * @param[in] paramCount count of parameters
 * @param[in] paramVal parameter value Array
 * @param[out] bRestartRadio1
 * @param[out] bRestartRadio2
 */
static void identifyRadioIndexToReset(int paramCount, parameterValStruct_t* val,BOOL *bRestartRadio1,BOOL *bRestartRadio2) 
{
	int x =0 ,index =0, SSID =0,apply_rf =0;
	for (x = 0; x < paramCount; x++)
	{
		WalPrint("val[%d].parameterName : %s\n",x,val[x].parameterName);
		if (!strncmp(val[x].parameterName, "Device.WiFi.Radio.1.", 20))
		{
			*bRestartRadio1 = TRUE;
		}
		else if (!strncmp(val[x].parameterName, "Device.WiFi.Radio.2.", 20))
		{
			*bRestartRadio2 = TRUE;
		}
		else
		{
			if ((!strncmp(val[x].parameterName, "Device.WiFi.SSID.", 17)))
			{
				sscanf(val[x].parameterName, "Device.WiFi.SSID.%d", &index);
				WalPrint("SSID index = %d\n", index);
				SSID = (1 << ((index) - 1));
				apply_rf = (2 - ((index) % 2));
				WalPrint("apply_rf = %d\n", apply_rf);

				if (apply_rf == 1)
				{
					*bRestartRadio1 = TRUE;
				}
				else if (apply_rf == 2)
				{
					*bRestartRadio2 = TRUE;
				}
			}
			else if (!strncmp(val[x].parameterName, "Device.WiFi.AccessPoint.",24))
			{
				sscanf(val[x].parameterName, "Device.WiFi.AccessPoint.%d", &index);
				WalPrint("AccessPoint index = %d\n", index);
				SSID = (1 << ((index) - 1));
				apply_rf = (2 - ((index) % 2));
				WalPrint("apply_rf = %d\n", apply_rf);

				if (apply_rf == 1)
				{
					*bRestartRadio1 = TRUE;
				}
				else if (apply_rf == 2)
				{
					*bRestartRadio2 = TRUE;
				}
			}
		}
	}
}

/**
 * @brief setParamAttributes Sets the parameter attribute value
 * @param[in] pParameterName parameter name
 * @param[in] attArr attribute value Array
 */
static int setParamAttributes(const char *pParameterName, const AttrVal *attArr)
{
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	int ret = 0, size = 0, notificationType = 0;

	componentStruct_t ** ppComponents = NULL;
	char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
	parameterAttributeStruct_t attriStruct;

	attriStruct.parameterName = NULL;
	attriStruct.notificationChanged = 1;
	attriStruct.accessControlChanged = 0;

	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	walStrncpy(paramName, pParameterName,sizeof(paramName));
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr), "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);

	IndexMpa_WEBPAtoCPE(paramName);
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem, /* prefix */
			&ppComponents, &size);

	if (ret == CCSP_SUCCESS && size == 1)
	{
		ret = CcspBaseIf_Register_Event(bus_handle, ppComponents[0]->componentName, "parameterValueChangeSignal");

		if (CCSP_SUCCESS != ret)
		{
			WalError("WebPa: CcspBaseIf_Register_Event failed!!!\n");
		}

		CcspBaseIf_SetCallback2(bus_handle, "parameterValueChangeSignal", ccspWebPaValueChangedCB, NULL);

		attriStruct.parameterName = paramName;
		notificationType = atoi(attArr->value);
		attriStruct.notification = notificationType;

		ret = CcspBaseIf_setParameterAttributes(bus_handle,	ppComponents[0]->componentName, ppComponents[0]->dbusPath, 0,
				&attriStruct, 1);

		if (CCSP_SUCCESS != ret)
		{
			WalError("Failed to SetValue for SetParamAttr ret : %d \n", ret);
		}

		free_componentStruct_t(bus_handle, size, ppComponents);
	}
	else
	{
		WalError("Component name is not supported ret : %d\n", ret);
	}

	return ret;
}

/**
 * @brief IndexMpa_WEBPAtoCPE maps to CPE index
 * @param[in] pParameterName parameter name
 */
 
static int IndexMpa_WEBPAtoCPE(char *pParameterName)
{
	int i = 0, j = 0, dmlNameLen = 0, instNum = 0, len = 0, matchFlag = -1;
	char pDmIntString[WIFI_MAX_STRING_LEN];
	char* instNumStart = NULL;
	char restDmlString[WIFI_MAX_STRING_LEN];
	for (i = 0; i < WIFI_PARAM_MAP_SIZE; i++)
	{
		dmlNameLen = strlen(CcspDmlName[i]);
		if (strncmp(pParameterName, CcspDmlName[i], dmlNameLen) == 0)
		{
			instNumStart = pParameterName + dmlNameLen;
			if (strlen(pParameterName) < dmlNameLen + 1)
			{
				// Found match on table, but there is no instance number
				break;
			}
			else
			{
				if (instNumStart[0] == '.')
				{
					instNumStart++;
				}
				else
				{ 
				  WalPrint("No matching index as instNumStart[0] : %c\n",instNumStart[0]);
				  break;
				}
				sscanf(instNumStart, "%d%s", &instNum, restDmlString);
				WalPrint("instNum : %d restDmlString : %s\n",instNum,restDmlString);

				// Find instance match and translate
				if (i == 0)
				{
					// For Device.WiFI.Radio.
					j = 0;
					len=2;
				}
				else
				{
					// For other than Device.WiFI.Radio.
					j = 2;
					len =WIFI_INDEX_MAP_SIZE;
				}
				for (j; j < len; j++)
				{
					if (IndexMap[j].WebPaInstanceNumber == instNum)
					{
						snprintf(pDmIntString, sizeof(pDmIntString),"%s.%d%s", CcspDmlName[i], IndexMap[j].CcspInstanceNumber, restDmlString);
						strcpy(pParameterName, pDmIntString);
						matchFlag = 1;
						break;
					}
				}
				WalPrint("matchFlag %d\n",matchFlag);
				if(matchFlag == -1)
				{
					WalError("Invalid index for : %s\n",pParameterName);
					return matchFlag;
				}
			}
			break;
		}
	}
	return 0;
}

/**
 * @brief IndexMpa_CPEtoWEBPA maps to WEBPA index
 * @param[in] pParameterName parameter name
 */
 
static void IndexMpa_CPEtoWEBPA(char **ppParameterName)
{
	int i = 0, j = 0, dmlNameLen = 0, instNum =0;
	char *pDmIntString = NULL;
	char* instNumStart = NULL;
	char restDmlString[WIFI_MAX_STRING_LEN];
	char *pParameterName = *ppParameterName;
	for (i = 0; i < WIFI_PARAM_MAP_SIZE; i++) 
	{
		dmlNameLen = strlen(CcspDmlName[i]);
		if (strncmp(pParameterName, CcspDmlName[i], dmlNameLen) == 0)
		{
			instNumStart = pParameterName + dmlNameLen;
			if (strlen(pParameterName) < dmlNameLen + 1)
			{
				// Found match on table, but there is no instance number
				break;
			}
			else
			{
				if (instNumStart[0] == '.')
				{
					instNumStart++;
				}
				sscanf(instNumStart, "%d%s", &instNum, restDmlString);
				// Find instance match and translate
				if (i == 0)
				{
					// For Device.WiFI.Radio.
					j = 0;
				} else {
					// For other than Device.WiFI.Radio.
					j = 2;
				}
				for (j; j < WIFI_INDEX_MAP_SIZE; j++)
				{
					if (IndexMap[j].CcspInstanceNumber == instNum)
					{
						pDmIntString = (char *) malloc(
								sizeof(char) * (dmlNameLen + MAX_PARAMETERNAME_LEN));
						if (pDmIntString)
						{
							snprintf(pDmIntString, dmlNameLen + MAX_PARAMETERNAME_LEN ,"%s.%d%s", CcspDmlName[i],
									IndexMap[j].WebPaInstanceNumber,
									restDmlString);
							WAL_FREE(pParameterName);
							*ppParameterName = pDmIntString;
							return;
						}

						break;
					}
				}
			}
			break;
		}
	}
	return;
}

/**
 * @brief getMatchingComponentValArrayIndex Compare objectName with the pre-populated ComponentValArray and return matching index
 *
 * param[in] objectName 
 * @return matching ComponentValArray index
 */
static int getMatchingComponentValArrayIndex(char *objectName)
{
	int i =0,index = -1;

	for(i = 0; i < compCacheSuccessCnt ; i++)
	{
		if(ComponentValArray[i].obj_name != NULL && !strcmp(objectName,ComponentValArray[i].obj_name))
		{
	      		index = ComponentValArray[i].comp_id;
			WalPrint("Matching Component Val Array index for object %s : %d\n",objectName, index);
			break;
		}	    
	}
	return index;
}


/**
 * @brief getMatchingSubComponentValArrayIndex Compare objectName with the pre-populated SubComponentValArray and return matching index
 *
 * param[in] objectName 
 * @return matching ComponentValArray index
 */
static int getMatchingSubComponentValArrayIndex(char *objectName)
{
	int i =0,index = -1;
	
	for(i = 0; i < subCompCacheSuccessCnt ; i++)
	{
		if(SubComponentValArray[i].obj_name != NULL && !strcmp(objectName,SubComponentValArray[i].obj_name))
		{
		      	index = SubComponentValArray[i].comp_id;
			WalPrint("Matching Sub-Component Val Array index for object %s : %d\n",objectName, index);
			break;
		}	    
	}	
	return index;
}

/**
 * @brief getObjectName Get object name from parameter name. Example WiFi from "Device.WiFi.SSID."
 *
 * @param[in] str Parameter Name
 * param[out] objectName Set with the object name
 * param[in] objectLevel Level of object 1, 2. Example 1 for WiFi and 2 for SSID
 */
static void getObjectName(char *str, char *objectName, int objectLevel)
{
	char *tmpStr;
	char localStr[MAX_PARAMETERNAME_LEN]={'\0'};
	walStrncpy(localStr,str,sizeof(localStr));
	int count = 1;
	
	if(localStr)
	{	
		tmpStr = strtok(localStr,".");
		
		while (tmpStr != NULL)
		{
			tmpStr = strtok (NULL, ".");
			if(tmpStr && count >= objectLevel)
			{
				strcpy(objectName,tmpStr);
				WalPrint("_________ objectName %s__________ \n",objectName);
	    		        break;
			}
			count++;
	  	}
	}
}


/**
 * @brief LOGInit Initialize RDK Logger
 */
void LOGInit()
{
	#ifdef FEATURE_SUPPORT_RDKLOG
		rdk_logger_init("/fss/gw/lib/debug.ini");    /* RDK logger initialization*/
	#endif
}


/**
 * @brief _WEBPA_LOG WEBPA RDK Logger API
 *
 * @param[in] level LOG Level
 * @param[in] msg Message to be logged 
 */
void _WEBPA_LOG(unsigned int level, const char *msg, ...)
{
	va_list arg;
	char *pTempChar = NULL;
	int ret = 0;
	unsigned int rdkLogLevel = LOG_DEBUG;

	switch(level)
	{
		case WEBPA_LOG_ERROR:
			rdkLogLevel = LOG_ERROR;
			break;

		case WEBPA_LOG_INFO:
			rdkLogLevel = LOG_INFO;
			break;

		case WEBPA_LOG_PRINT:
			rdkLogLevel = LOG_DEBUG;
			break;
	}

	if( rdkLogLevel <= LOG_INFO )
	{
		pTempChar = (char *)malloc(4096);
		if(pTempChar)
		{
			va_start(arg, msg);
			ret = vsnprintf(pTempChar, 4096, msg,arg);
			if(ret < 0)
			{
				perror(pTempChar);
			}
			va_end(arg);
			RDK_LOG(rdkLogLevel, "LOG.RDK.WEBPA", "%s", pTempChar);
			WAL_FREE(pTempChar);
		}
	}
}

/**
 * @brief getWebPAConfig interface returns the WebPA config data.
 *
 * @param[in] param WebPA config param name.
 * @return const char* WebPA config param value.
 */
const char* getWebPAConfig(WCFG_PARAM_NAME param)
{
	const char *ret = NULL;
	
	switch(param)
	{
		case WCFG_COMPONENT_NAME:
			ret = RDKB_WEBPA_COMPONENT_NAME;
			break;

		case WCFG_CFG_FILE:
			ret = RDKB_WEBPA_CFG_FILE;
			break;

		case WCFG_CFG_FILE_SRC:
			ret = RDKB_WEBPA_CFG_FILE_SRC;
			break;

		case WCFG_DEVICE_MAC:
			ret = RDKB_WEBPA_DEVICE_MAC;
			break;

		case WCFG_XPC_SYNC_PARAM_CID:
			ret = RDKB_XPC_SYNC_PARAM_CID;
			break;

		case WCFG_XPC_SYNC_PARAM_CMC:
			ret = RDKB_XPC_SYNC_PARAM_CMC;
			break;

		case WCFG_XPC_SYNC_PARAM_SPV:
			ret = RDKB_XPC_SYNC_PARAM_SPV;
			break;

		default:
			ret = STR_NOT_DEFINED;
	}
	
	return ret;
}

/**
 * @brief WALInit Initialize WAL
 */
void WALInit()
{
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	int ret = 0, i = 0, size = 0, len = 0, cnt = 0, cnt1 = 0, retryCount = 0;
	char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
	componentStruct_t ** ppComponents = NULL;

	walStrncpy(l_Subsystem, "eRT.",sizeof(l_Subsystem));
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr),"%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);

	WalPrint("-------- Start of populateComponentValArray -------\n");
	len = sizeof(objectList)/sizeof(objectList[0]);
	WalPrint("Length of object list : %d\n",len);
	for(i = 0; i < len ; i++)
	{
		walStrncpy(paramName,objectList[i],sizeof(paramName));
		do
		{
			ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
					dst_pathname_cr, paramName, l_Subsystem, &ppComponents, &size);
			
			if (ret == CCSP_SUCCESS)
			{	    
				retryCount = 0;
				// Allocate memory for ComponentVal obj_name, comp_name, dbus_path
				ComponentValArray[cnt].obj_name = (char *)malloc(sizeof(char) * (MAX_PARAMETERNAME_LEN/2));
				ComponentValArray[cnt].comp_name = (char *)malloc(sizeof(char) * (MAX_PARAMETERNAME_LEN/2));
				ComponentValArray[cnt].dbus_path = (char *)malloc(sizeof(char) * (MAX_PARAMETERNAME_LEN/2));

				ComponentValArray[cnt].comp_id = cnt;
				ComponentValArray[cnt].comp_size = size;
				getObjectName(paramName,ComponentValArray[cnt].obj_name,1);
				walStrncpy(ComponentValArray[cnt].comp_name,ppComponents[0]->componentName,MAX_PARAMETERNAME_LEN/2);
				walStrncpy(ComponentValArray[cnt].dbus_path,ppComponents[0]->dbusPath,MAX_PARAMETERNAME_LEN/2);
					   
				WalInfo("ComponentValArray[%d].comp_id = %d,ComponentValArray[cnt].comp_size = %d, ComponentValArray[%d].obj_name = %s, ComponentValArray[%d].comp_name = %s, ComponentValArray[%d].dbus_path = %s\n", cnt, ComponentValArray[cnt].comp_id,ComponentValArray[cnt].comp_size, cnt, ComponentValArray[cnt].obj_name, cnt, ComponentValArray[cnt].comp_name, cnt, ComponentValArray[cnt].dbus_path);  
				cnt++;
			}
			else
			{
				retryCount++;
				WalError("------------Failed to get component info for object %s----------: ret = %d, size = %d, retrying .... %d ...\n", objectList[i], ret, size, retryCount);
				if(retryCount == WAL_COMPONENT_INIT_RETRY_COUNT)
				{
					WalError("Unable to get component for object %s\n", objectList[i]);
				}
				else
				{
					sleep(WAL_COMPONENT_INIT_RETRY_INTERVAL);
				}
			}
			free_componentStruct_t(bus_handle, size, ppComponents);

		}while((retryCount >= 1) && (retryCount <= 3));		
	}
	
	compCacheSuccessCnt = cnt - 1;
	WalPrint("compCacheSuccessCnt : %d\n", compCacheSuccessCnt);
	
	WalPrint("Initializing sub component list\n");
	len = sizeof(subObjectList)/sizeof(subObjectList[0]);
	WalPrint("Length of sub object list : %d\n",len);

	retryCount = 0;
	for(i = 0; i < len; i++)
	{
		walStrncpy(paramName,subObjectList[i],sizeof(paramName));
		do
		{
			ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
					dst_pathname_cr, paramName, l_Subsystem, &ppComponents, &size);
			
			if (ret == CCSP_SUCCESS)
			{	       
				retryCount = 0;
				// Allocate memory for ComponentVal obj_name, comp_name, dbus_path
				SubComponentValArray[cnt1].obj_name = (char *)malloc(sizeof(char) * (MAX_PARAMETERNAME_LEN/2));
				SubComponentValArray[cnt1].comp_name = (char *)malloc(sizeof(char) * (MAX_PARAMETERNAME_LEN/2));
				SubComponentValArray[cnt1].dbus_path = (char *)malloc(sizeof(char) * (MAX_PARAMETERNAME_LEN/2));

				SubComponentValArray[cnt1].comp_id = cnt1;
				SubComponentValArray[cnt1].comp_size = size;
				getObjectName(paramName,SubComponentValArray[cnt1].obj_name,2);
				WalPrint("in WALInit() SubComponentValArray[cnt].obj_name is %s",SubComponentValArray[cnt1].obj_name);
				walStrncpy(SubComponentValArray[cnt1].comp_name,ppComponents[0]->componentName,MAX_PARAMETERNAME_LEN/2);
				walStrncpy(SubComponentValArray[cnt1].dbus_path,ppComponents[0]->dbusPath,MAX_PARAMETERNAME_LEN/2);
					   
				WalInfo("SubComponentValArray[%d].comp_id = %d,SubComponentValArray[i].comp_size = %d, SubComponentValArray[%d].obj_name = %s, SubComponentValArray[%d].comp_name = %s, SubComponentValArray[%d].dbus_path = %s\n", cnt1, SubComponentValArray[cnt1].comp_id,SubComponentValArray[cnt1].comp_size, cnt1, SubComponentValArray[cnt1].obj_name, cnt1, SubComponentValArray[cnt1].comp_name, cnt1, SubComponentValArray[cnt1].dbus_path);  
				cnt1++;
			}
			else
			{
				retryCount++;
				WalError("------------Failed to get component info for object %s----------: ret = %d, size = %d, retrying.... %d ....\n", subObjectList[i], ret, size, retryCount);
				if(retryCount == WAL_COMPONENT_INIT_RETRY_COUNT)
				{
					WalError("Unable to get component for object %s\n", subObjectList[i]);
				}
				else
				{
					sleep(WAL_COMPONENT_INIT_RETRY_INTERVAL);
				}
			}
			free_componentStruct_t(bus_handle, size, ppComponents);

		}while((retryCount >= 1) && (retryCount <= (WAL_COMPONENT_INIT_RETRY_COUNT - 1)));
	}

	subCompCacheSuccessCnt = cnt1;
	WalPrint("subCompCacheSuccessCnt : %d\n", subCompCacheSuccessCnt);
	WalPrint("-------- End of populateComponentValArray -------\n");

	// Initialize wifiCompName variable
	if(ComponentValArray[0].comp_name != NULL)
	{
		wifiCompName = ComponentValArray[0].comp_name;
		WalPrint("wifiCompName %s\n", wifiCompName);
	}

	// Initialize Apply WiFi Settings handler
	initApplyWiFiSettings();
}

