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

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define WIFI_INDEX_MAP_SIZE                     18
#define WIFI_PARAM_MAP_SIZE                     3
#define WIFI_MAX_STRING_LEN                     128
#define MAX_PARAMETERNAME_LEN					512
#define MAX_PARAMETERVALUE_LEN					512
#define MAX_DBUS_INTERFACE_LEN					32
#define MAX_PATHNAME_CR_LEN						64
#define CCSP_COMPONENT_ID_WebPA    				0x0000000A
/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
typedef struct
{
	ULONG WebPaInstanceNumber;
	ULONG CcspInstanceNumber;
}CpeWebpaIndexMap;

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
void (*fp_stack)(ParamNotify*);
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

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static WAL_STATUS mapStatus(int ret);
static void ccspWebPaValueChangedCB(parameterSigStruct_t* val, int size,void* user_data);
static int getParamValues(char *pParameterName, ParamVal ***parametervalArr,int *TotalParams);
static int getParamAttributes(char *pParameterName, AttrVal ***attr, int *TotalParams);
static int setParamValues(ParamVal paramVal[], int paramCount, const unsigned int isAtomic, int * setRet);
static int setParamAttributes(const char *pParameterName, const AttrVal *attArr);
static int prepare_parameterValueStruct(parameterValStruct_t* val, ParamVal *paramVal, char *paramName);
static void identifyRadioIndexToReset(int paramCount, ParamVal *paramVal,BOOL *bRestartRadio1,BOOL *bRestartRadio2);
static void IndexMpa_WEBPAtoCPE(char *pParameterName);
static void IndexMpa_CPEtoWEBPA(char **ppParameterName);


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
	fp_stack = cb;
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
	int cnt = 0;
	int ret = 0;
	for (cnt = 0; cnt < paramCount; cnt++)
	{
		ret = getParamValues(paramName[cnt], &paramValArr[cnt], &retValCount[cnt]);
		retStatus[cnt] = mapStatus(ret);
		WalPrint("Parameter Name: %s, Parameter Value return: %d\n",paramName[cnt],retStatus[cnt]);
	}
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
void getAttributes(const char *paramName[], const unsigned int paramCount, AttrVal ***attr,
		int *retAttrCount, WAL_STATUS *retStatus)
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
 * @brief setValues Returns the status from stack for SET request
 *
 * @param[in] paramName parameter Name
 * @param[in] paramCount Number of parameters
 * @param[in] isAtomic set for atomic set
 * @param[out] retStatus Returns parameter Value from the stack
 */

void setValues(const ParamVal paramVal[], const unsigned int paramCount, const unsigned int isAtomic, WAL_STATUS *retStatus)
{
	int cnt = 0, ret = 0;
	int * setRet = (int *) malloc(sizeof(int) * paramCount);
	ret = setParamValues(paramVal,paramCount,isAtomic,setRet);
	for (cnt = 0; cnt < paramCount; cnt++) 
	{
		if(isAtomic)
		{
			retStatus[cnt] = mapStatus(ret);
		}
		else
		{
			retStatus[cnt] = mapStatus(setRet[cnt]);
		}
	}
	
	if(setRet)
	{
		free(setRet);
	}
	setRet = NULL;
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

	ParamNotify paramNotify;
	paramNotify.paramName = val->parameterName;
	paramNotify.oldValue= val->oldValue;
	paramNotify.newValue = val->newValue;
	paramNotify.type = val->type;
	paramNotify.writeID = val->writeID;
	
	WalPrint("Notification Event from stack: Parameter Name: %s, Old Value: %s, New Value: %s, Data Type: %d, Write ID: %d\n", paramNotify.paramName, paramNotify.oldValue, paramNotify.newValue, paramNotify.type, paramNotify.writeID);
	
	(*fp_stack)(&paramNotify);
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
	parameterValStruct_t **parametervalError = NULL;
	strcpy(l_Subsystem, "eRT.");
	strcpy(paramName, pParameterName);
	sprintf(dst_pathname_cr, "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);
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
			parametervalError = (parameterValStruct_t **) malloc(
						sizeof(parameterValStruct_t *) * 1);
			parametervalError[0] = (parameterValStruct_t *) malloc(
					sizeof(parameterValStruct_t) * 1);
			parametervalError[0]->parameterValue = "ERROR";
			parametervalError[0]->parameterName = pParameterName;
			parametervalError[0]->type = ccsp_string;
			*parametervalArr = parametervalError;
			*TotalParams = 1;
			WalError("Error:Failed to GetValue for param: %s ret: %d\n", paramName, ret);

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
		}
	}
	else
	{
		parametervalError = (parameterValStruct_t **) malloc(
					sizeof(parameterValStruct_t *) * 1);
		parametervalError[0] = (parameterValStruct_t *) malloc(
				sizeof(parameterValStruct_t));
		parametervalError[0]->parameterValue = "ERROR";
		parametervalError[0]->parameterName = pParameterName;
		parametervalError[0]->type = ccsp_string;
		*parametervalArr = parametervalError;
		*TotalParams = 1;
		WalError("Error: Parameter name is not supported.ret : %d\n", ret);
	}
	free_componentStruct_t(bus_handle, size, ppComponents);
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
	strcpy(l_Subsystem, "eRT.");
	strcpy(paramName, pParameterName);
	sprintf(dst_pathname_cr, "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);

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
			strcpy(attr[0][0]->name, pParameterName);
			attr[0][0]->type = WAL_INT;
			*TotalParams = 1;
			WalError("Error:Failed to GetValue for GetParamAttr ret : %d \n", ret);
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
				strcpy(attr[0][x]->name, ppAttrArray[x]->parameterName);
				sprintf(attr[0][x]->value, "%d", ppAttrArray[x]->notification);
				attr[0][x]->type = WAL_INT;
			}
		}
		free_parameterAttributeStruct_t(bus_handle, sizeAttrArr, ppAttrArray);
		free_componentStruct_t(bus_handle, size, ppComponents);
	}
	else
	{
		attr[0] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
		attr[0][0] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
		attr[0][0]->name = (char *) malloc(sizeof(char) * MAX_PARAMETERNAME_LEN);
		attr[0][0]->value = (char *) malloc(sizeof(char) * MAX_PARAMETERVALUE_LEN);
		sprintf(attr[0][0]->value, "%d", -1);
		strcpy(attr[0][0]->name, pParameterName);
		attr[0][0]->type = WAL_INT;
		*TotalParams = 1;
		WalError("Error: Parameter name is not supported.ret : %d\n", ret);
	}
	return ret;
}

/**
 * @brief setParamValues Returns the status from stack for SET request
 *
 * @param[in] paramVal parameter value Array
 * @param[in] paramCount Number of parameters
 * @param[in] isAtomic set for atomic set
 */

static int setParamValues(ParamVal paramVal[], int paramCount, const unsigned int isAtomic,int * setRet)
{
	char* faultParam = NULL;
	char dst_pathname_cr[MAX_PATHNAME_CR_LEN] = { 0 };
	char l_Subsystem[MAX_DBUS_INTERFACE_LEN] = { 0 };
	int ret=0, size = 0, cnt = 0, cnt1=0;
	componentStruct_t ** ppComponents = NULL;
	char CompName[MAX_PARAMETERNAME_LEN/2] = { 0 };
	char paramName[MAX_PARAMETERNAME_LEN] = { 0 };
	BOOL bRadioRestartEn = (BOOL)FALSE;

	//parameters for atomic 
	BOOL bRestartRadio1 = FALSE;
	BOOL bRestartRadio2 = FALSE;
	int nreq = 0;
	
	strcpy(l_Subsystem, "eRT.");
	sprintf(dst_pathname_cr, "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);
	parameterValStruct_t *RadApplyParam = NULL;
	parameterValStruct_t val_set[4] = { { "Device.WiFi.Radio.1.X_CISCO_COM_ApplySettingSSID","1", ccsp_int}, 						{ "Device.WiFi.Radio.1.X_CISCO_COM_ApplySetting", "true", ccsp_boolean},
					{ "Device.WiFi.Radio.2.X_CISCO_COM_ApplySettingSSID","2", ccsp_int},
					{ "Device.WiFi.Radio.2.X_CISCO_COM_ApplySetting", "true", ccsp_boolean}};
	parameterValStruct_t* val = (parameterValStruct_t*) malloc(sizeof(parameterValStruct_t) * paramCount);
	
	if(!isAtomic)
	{
		for (cnt = 0; cnt < paramCount; cnt++) 
		{
			strcpy(paramName, paramVal[cnt].name);
			IndexMpa_WEBPAtoCPE(paramName);
			val[cnt].parameterName = NULL;
			ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
					dst_pathname_cr, paramName, l_Subsystem, 
					&ppComponents, &size);
	
			if (ret == CCSP_SUCCESS && size == 1) 
			{
				ret = prepare_parameterValueStruct(&val[cnt], &paramVal[cnt], paramName);
				if(ret)
				{
					setRet[cnt] = ret;
					WalError("Error:Preparing parameter value struct is failed. \n");
					continue;
				}

				ret = CcspBaseIf_setParameterValues(bus_handle,	ppComponents[0]->componentName, ppComponents[0]->dbusPath, 					0,CCSP_COMPONENT_ID_WebPA, &val[cnt], 1, TRUE, &faultParam);

				if (ret != CCSP_SUCCESS && faultParam)
				{
					setRet[cnt] = ret;
					WalError("Error:Failed to SetValue for param  '%s' ret : %d \n", faultParam, ret);
					free_componentStruct_t(bus_handle, size, ppComponents);
					continue;
				}
				
				setRet[cnt] = ret;
				free_componentStruct_t(bus_handle, size, ppComponents);
			}
			else 
			{
				setRet[cnt] = ret;
				WalError("Error: Parameter name is not supported.ret : %d\n", ret);
				continue;
			} 			
		}
		if (faultParam) 
		{
			free(faultParam);
		}
		faultParam = NULL;
	}
	else
	{
		strcpy(paramName, paramVal[0].name);
		IndexMpa_WEBPAtoCPE(paramName);
		ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
				dst_pathname_cr, paramName, l_Subsystem, 
				&ppComponents, &size);
		if(ret != CCSP_SUCCESS)
		{
			WalError("Error: Parameter name is not supported.ret : %d\n", ret);
			if (val) 
			{
				free(val);
			}
			val = NULL;
			return ret;
		}
		strcpy(CompName, ppComponents[0]->componentName);
		WalPrint("CompName = %s,paramCount = %d\n", CompName,paramCount);
		if(!strcmp(CompName,"eRT.com.cisco.spvtg.ccsp.wifi")) 
		{
			bRadioRestartEn = TRUE;
		}
		

		for (cnt = 0; cnt < paramCount; cnt++) 
		{
			strcpy(paramName, paramVal[cnt].name);
			IndexMpa_WEBPAtoCPE(paramName);

			ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle, dst_pathname_cr, paramName, l_Subsystem,
					&ppComponents, &size);
	
			

			if (ret == CCSP_SUCCESS && size == 1) 
			{
			
				if (strcmp(CompName, ppComponents[0]->componentName) != 0)
				{
					WalError("Error: Parameters does not belong to the same component\n");
					if (val) 
					{
						free(val);
					}
					val = NULL;
					return CCSP_FAILURE;
				}
				
				ret = prepare_parameterValueStruct(&val[cnt], &paramVal[cnt], paramName);
				if(ret)
				{
					WalError("Error:Preparing parameter value struct is Failed \n");
				}
			}
			else 
			{
				WalError("Error: Parameter name is not supported.ret : %d\n", ret);
				if (val) 
				{
					free(val);
				}
				val = NULL;
				return ret;
			} 			
		}
		
		ret = CcspBaseIf_setParameterValues(bus_handle,ppComponents[0]->componentName, ppComponents[0]->dbusPath, 0,CCSP_COMPONENT_ID_WebPA, val, paramCount, TRUE, &faultParam);
		
		if (ret != CCSP_SUCCESS && faultParam) 
		{
			WalError("Error:Failed to SetAtomicValue for param  '%s' ret : %d \n", faultParam, ret);
			if (val) 
			{
				free(val);
			}
			val = NULL;
		
			if (faultParam) 
			{
				free(faultParam);
			}		
			faultParam = NULL;
			free_componentStruct_t(bus_handle, size, ppComponents);
			return ret;
		}

		//Identify the rario and apply settings
		if(bRadioRestartEn)
		{
			bRadioRestartEn = FALSE;
			identifyRadioIndexToReset(paramCount,paramVal,&bRestartRadio1,&bRestartRadio2);
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

			ret = CcspBaseIf_setParameterValues(bus_handle, ppComponents[0]->componentName, ppComponents[0]->dbusPath,
				0, CCSP_COMPONENT_ID_WebPA,	RadApplyParam, nreq, TRUE,&faultParam);
			if (ret != CCSP_SUCCESS && faultParam) 
			{
				WalError("Failed to Set Apply Settings\n");
				if (faultParam) 
				{
					free(faultParam);
				}
				faultParam = NULL;
			}
		}

		free_componentStruct_t(bus_handle, size, ppComponents);
		
		if (faultParam) 
		{
			free(faultParam);
		}		
		faultParam = NULL;
	}
	
	if (val) 
	{
		free(val);
	}
	val = NULL;
	return ret;
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
	val->parameterName = paramName;
	val->parameterValue = paramVal->value;

	if(val->parameterName == NULL)
	{
		return WAL_FAILURE;
	}
		
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
static void identifyRadioIndexToReset(int paramCount, ParamVal *paramVal,BOOL *bRestartRadio1,BOOL *bRestartRadio2) 
{
	int x =0 ,index =0, SSID =0,apply_rf =0;
	for (x = 0; x < paramCount; x++)
	{
		if (!strncmp(paramVal[x].name, "Device.WiFi.Radio.1.", 20))
		{
			*bRestartRadio1 = TRUE;
		}
		else if (!strncmp(paramVal[x].name, "Device.WiFi.Radio.2.", 20))
		{
			*bRestartRadio2 = TRUE;
		}
		else
		{
			if ((!strncmp(paramVal[x].name, "Device.WiFi.SSID.", 17)))
			{
				sscanf(paramVal[x].name, "Device.WiFi.SSID.%d", &index);
				WalPrint("index = %d\n", index);
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
			else if (!strncmp(paramVal[x].name, "Device.WiFi.AccessPoint.",24))
			{
				sscanf(paramVal[x].name, "Device.WiFi.AccessPoint.%d", &index);
				SSID = (1 << ((index) - 1));
				apply_rf = (2 - ((index) % 2));

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
 * @brief setParamValues Returns the status from stack for SET request
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

	strcpy(l_Subsystem, "eRT.");
	strcpy(paramName, pParameterName);
	sprintf(dst_pathname_cr, "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);

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
			WalError("Error: Failed to SetValue for SetParamAttr ret : %d \n", ret);
		}

		free_componentStruct_t(bus_handle, size, ppComponents);
	}
	else
	{
		WalError("Error: Component name is not supported ret : %d\n", ret);
	}

	return ret;
}

/**
 * @brief IndexMpa_WEBPAtoCPE maps to CPE index
 * @param[in] pParameterName parameter name
 */
 
static void IndexMpa_WEBPAtoCPE(char *pParameterName)
{
	int i = 0, j = 0, dmlNameLen = 0, instNum = 0;
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
				sscanf(instNumStart, "%d%s", &instNum, restDmlString);

				// Find instance match and translate
				if (i == 0)
				{
					// For Device.WiFI.Radio.
					j = 0;
				}
				else
				{
					// For other than Device.WiFI.Radio.
					j = 2;
				}
				for (j; j < WIFI_INDEX_MAP_SIZE; j++)
				{
					if (IndexMap[j].WebPaInstanceNumber == instNum)
					{
						sprintf(pDmIntString, "%s.%d%s", CcspDmlName[i], IndexMap[j].CcspInstanceNumber, restDmlString);
						strcpy(pParameterName, pDmIntString);
						break;
					}
				}
			}
			break;
		}
	}
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
							sprintf(pDmIntString, "%s.%d%s", CcspDmlName[i],
									IndexMap[j].WebPaInstanceNumber,
									restDmlString);
							free(pParameterName);
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
