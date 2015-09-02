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
#define CCSP_ERR_WILDCARD_NOT_SUPPORTED         110 //TODO: Why is it defined here?

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
static void ccspWebPaValueChangedCB(ParamNotify *paramNotify, int size,void* user_data);
static int getParamValues(char *pParameterName, ParamVal ***parametervalArr,int *TotalParams);
static int getParamAttributes(char *pParameterName, AttrVal ***attr, int *TotalParams);
static int setParamValues(ParamVal paramVal);
static int setAtomicParamValues(ParamVal paramVal[], int paramCount);
static int setParamAttributes(const char *pParameterName, const AttrVal *attArr);
static void IndexMpa_WEBPAtoCPE(char *pParameterName);
static char* IndexMpa_CPEtoWEBPA(char *pParameterName);

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
	int i = 0;
	int ret = 0;
	for (cnt = 0; cnt < paramCount; cnt++) {
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

	for (cnt = 0; cnt < paramCount; cnt++) {
		ret = getParamAttributes(paramName[cnt], &attr[cnt], &retAttrCount[cnt]);
		retStatus[cnt] = mapStatus(ret);
		WalPrint("Parameter Name: %s, Parameter Attributes return: %d\n",paramName[cnt],retStatus[cnt]);
	}
}

/**
 * @brief setValues Returns the parameter Names from stack for GET request
 *
 * @param[in] paramName parameter Name
 * @param[in] paramCount Number of parameters
 * @param[out] retStatus Returns parameter Value from the stack
 */
void setValues(const ParamVal paramVal[], const unsigned int paramCount, const unsigned int isAtomic, WAL_STATUS *retStatus)
{
	int cnt = 0, ret = 0;

	if(false == isAtomic) {
		for (cnt = 0; cnt < paramCount; cnt++) {
			ret = setParamValues(paramVal[cnt]);
			retStatus[cnt] = mapStatus(ret);
		}
	}
	else {
		ret = setAtomicParamValues(paramVal,paramCount);
		for (cnt = 0; cnt < paramCount; cnt++) 
		{
			retStatus[cnt] = mapStatus(ret);
		}
	}
}

void setAttributes(const char *paramName[], const unsigned int paramCount,
		const AttrVal *attArr[], WAL_STATUS *retStatus)
{
	int cnt = 0;
	int ret = 0;

	for (cnt = 0; cnt < paramCount; cnt++) {
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
		case CCSP_ERR_WILDCARD_NOT_SUPPORTED:
			return WAL_ERR_WILDCARD_NOT_SUPPORTED;
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

static void ccspWebPaValueChangedCB(ParamNotify *paramNotify, int size, void* user_data)
{
	(*fp_stack)(paramNotify);
}

/**
 * @brief getParamValues Returns the parameter Values from stack for GET request
 *
 * @param[in] paramName parameter Name
 * @param[in] paramCount Number of parameters
 * @param[in] paramValArr parameter value Array
 * @param[out] TotalParams Number of parameters returned from stack
 */
static int getParamValues(char *pParameterName, ParamVal ***parametervalArr, int *TotalParams)
{
	char dst_pathname_cr[64] = { 0 };
	char l_Subsystem[32] = { 0 };
	int ret;
	int size = 0;
	char *temp = NULL;
	componentStruct_t ** ppComponents = NULL;
	char paramName[100] = { 0 };
	char *p = &paramName;
	parameterValStruct_t **parameterval = NULL;
	strcpy(l_Subsystem, "eRT.");
	strcpy(paramName, pParameterName);
	sprintf(dst_pathname_cr, "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);
	IndexMpa_WEBPAtoCPE(paramName);
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem, &ppComponents, &size);

	parameterValStruct_t **parametervalError = (parameterValStruct_t **) malloc(
			sizeof(parameterValStruct_t *) * 1);
	if (ret == CCSP_SUCCESS && size == 1) {
		char *parameterNames[1];
		int val_size = 0;
		parameterNames[0] = p;

		ret = CcspBaseIf_getParameterValues(bus_handle,
				ppComponents[0]->componentName, ppComponents[0]->dbusPath,
				parameterNames, //paramName,
				1, &val_size, &parameterval);

		if (ret != CCSP_SUCCESS) {
			parametervalError[0] = (parameterValStruct_t *) malloc(
					sizeof(parameterValStruct_t) * 1);
			parametervalError[0]->parameterValue = "ERROR";
			temp = IndexMpa_CPEtoWEBPA(pParameterName);
			if (temp) {
				if(pParameterName) {
					free(pParameterName);
				}
				pParameterName = temp;
			}
			parametervalError[0]->parameterName = pParameterName;
			parametervalError[0]->type = ccsp_string;
			*parametervalArr = parametervalError;
			*TotalParams = 1;
			WalError("Error1:Failed to GetValue for param %s ~~~~ ret: %d\n", paramName, ret);

		}
		else {
			*TotalParams = val_size;
			int i;
			parametervalArr[0] = (ParamVal **) malloc(sizeof(ParamVal*) * val_size);
			for (i = 0; i < val_size; i++) {
				temp = IndexMpa_CPEtoWEBPA(parameterval[i]->parameterName);
				if (temp) {
					if(parameterval[i]->parameterName) {
						free(parameterval[i]->parameterName);
					}
					parameterval[i]->parameterName = temp;
				}
				parametervalArr[0][i] = parameterval[i];
				WalPrint("success: %s %s %d \n",parametervalArr[0][i]->name,parametervalArr[0][i]->value,parametervalArr[0][i]->type);
			}

		}
	}
	else {
		parametervalError[0] = (parameterValStruct_t *) malloc(
				sizeof(parameterValStruct_t));
		parametervalError[0]->parameterValue = "ERROR";
		temp = IndexMpa_CPEtoWEBPA(pParameterName);
		if (temp) {
				if(pParameterName) {
					free(pParameterName);
				}
				pParameterName = temp;
		}
		parametervalError[0]->parameterName = pParameterName;
		parametervalError[0]->type = ccsp_string;
		*parametervalArr = parametervalError;
		*TotalParams = 1;
		WalError("Error3:Failed to GetValue for param %s ~~~~ ret: %d\n", paramName, ret);
	}
	return ret;
}

static int getParamAttributes(char *pParameterName, AttrVal ***attr, int *TotalParams)
{
	char dst_pathname_cr[64] = { 0 };
	char l_Subsystem[32] = { 0 };
	int ret;
	int size = 0;
	char * temp =NULL;
	componentStruct_t ** ppComponents = NULL;
	char paramName[100] = { 0 };
	int sizeAttrArr = 0;
	char *p = &paramName;
	parameterAttributeStruct_t** ppAttrArray = NULL;
	strcpy(l_Subsystem, "eRT.");
	strcpy(paramName, pParameterName);
	sprintf(dst_pathname_cr, "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);
	IndexMpa_WEBPAtoCPE(paramName);
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem,  //prefix
			&ppComponents, &size);

	if (ret == CCSP_SUCCESS && size == 1) {
		char *parameterNames[1];
		parameterNames[0] = p;

		ret = CcspBaseIf_getParameterAttributes(bus_handle,
				ppComponents[0]->componentName, ppComponents[0]->dbusPath,
				parameterNames, 1, &sizeAttrArr, &ppAttrArray);

		if (CCSP_SUCCESS != ret) {
			attr[0] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
			attr[0][0] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
			attr[0][0]->name = (char *) malloc(sizeof(char) * 100);
			attr[0][0]->value = (char *) malloc(sizeof(char) * 100);
			temp = IndexMpa_CPEtoWEBPA(pParameterName);
			if (temp)
		 	{
				if(pParameterName)
				{
					free(pParameterName);
				}
				pParameterName = temp;
			}
			sprintf(attr[0][0]->value, "%d", -1);
			strcpy(attr[0][0]->name, pParameterName);
			attr[0][0]->type = WAL_INT;
			*TotalParams = 1;
			CcspTraceDebug(("CcspBaseIf_setParameterAttributes (turn notification on) failed!!!\n"));
			WalError("Error:Failed to GetValue for GetParamAttr ret : %d \n", ret);
		}
		else {
			int x = 0;
			*TotalParams = sizeAttrArr;
			attr[0] = (AttrVal *) malloc(sizeof(AttrVal) * sizeAttrArr);
			for (x = 0; x < sizeAttrArr; x++) {
				attr[0][x] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
				attr[0][x]->name = (char *) malloc(sizeof(char) * 100);
				attr[0][x]->value = (char *) malloc(sizeof(char) * 100);


				temp = IndexMpa_CPEtoWEBPA(ppAttrArray[x]->parameterName);
				if (temp) {
					if(ppAttrArray[x]->parameterName){
						free(ppAttrArray[x]->parameterName);
					}
					ppAttrArray[x]->parameterName = temp;
				}
				strcpy(attr[0][x]->name, ppAttrArray[x]->parameterName);
				sprintf(attr[0][x]->value, "%d", ppAttrArray[x]->notification);
				attr[0][x]->type = WAL_INT;
			}
		}
		free_componentStruct_t(bus_handle, size, ppComponents);
	} else {
		attr[0] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
		attr[0][0] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
		attr[0][0]->name = (char *) malloc(sizeof(char) * 100);
		attr[0][0]->value = (char *) malloc(sizeof(char) * 100);
		temp = IndexMpa_CPEtoWEBPA(pParameterName);
		if (temp)
		 {
				if(pParameterName)
				{
					free(pParameterName);
				}
				pParameterName = temp;
		}
		sprintf(attr[0][0]->value, "%d", -1);
		strcpy(attr[0][0]->name, pParameterName);
		attr[0][0]->type = WAL_INT;
		*TotalParams = 1;
		WalError("Error:Failed to GetValue for GetParamAttr ~~~ ret : %d \n", ret);
	}
	return ret;
}

static int setParamValues(ParamVal paramVal)
{
	char* faultParam = NULL;
	char dst_pathname_cr[64] = { 0 };
	extern ANSC_HANDLE bus_handle;
	char l_Subsystem[32] = { 0 };
	int ret;
	int size = 0;
	componentStruct_t ** ppComponents = NULL;
	char value[100] = { 0 };
	char paramName[100] = { 0 };
	strcpy(l_Subsystem, "eRT.");
	strcpy(paramName, paramVal.name);
	strcpy(value, paramVal.value);
	sprintf(dst_pathname_cr, "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);
	IndexMpa_WEBPAtoCPE(paramName);
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem, /* prefix */
			&ppComponents, &size);

	if (ret == CCSP_SUCCESS && size == 1) {
		parameterValStruct_t val[1] = { { "Device.LogAgent.WifiLogMsg", "hello", ccsp_string } };

		val[0].parameterName = paramName;
		val[0].parameterValue = value;

		if (paramVal.type == 0) {
			val[0].type = ccsp_string;
		} else if (paramVal.type == 1) {
			val[0].type = ccsp_int;
		} else if (paramVal.type == 2) {
			val[0].type = ccsp_unsignedInt;
		} else if (paramVal.type == 3) {
			val[0].type = ccsp_boolean;
		} else if (paramVal.type == 4) {
			val[0].type = ccsp_dateTime;
		} else if (paramVal.type == 5) {
			val[0].type = ccsp_base64;
		} else if (paramVal.type == 6) {
			val[0].type = ccsp_long;
		} else if (paramVal.type == 7) {
			val[0].type = ccsp_unsignedLong;
		} else if (paramVal.type == 8) {
			val[0].type = ccsp_float;
		} else if (paramVal.type == 9) {
			val[0].type = ccsp_double;
		} else if (paramVal.type == 10) {
			val[0].type = ccsp_byte;
		} else {
			val[0].type = ccsp_none;
		}

		ret = CcspBaseIf_setParameterValues(bus_handle,
				ppComponents[0]->componentName, ppComponents[0]->dbusPath, 0,
				0x0, /* session id and write id */
				&val, 1, TRUE, /* no commit */
				&faultParam);

		if (ret != CCSP_SUCCESS && faultParam) {
			AnscTraceError(("Error:Failed to SetValue for param '%s'\n", faultParam));
			WalError("Error:Failed to SetValue for param  '%s' ret : %d \n", faultParam, ret);
		}

	} else {
		WalError("Error:Failed to SetValue for param  '%s' ret : %d \n", faultParam, ret);
	}
	return ret;
}

static int setAtomicParamValues(ParamVal paramVal[], int paramCount)
{
	char* faultParam = NULL;
	char dst_pathname_cr[64] = { 0 };
	extern ANSC_HANDLE bus_handle;
	char l_Subsystem[32] = { 0 };
	int ret;
	int size = 0, cnt = 0;
	componentStruct_t ** ppComponents = NULL;
	char CompName[256] = { 0 };
	char paramName[512] = { 0 };
	BOOL bRadioRestartEn = (BOOL )FALSE;
	strcpy(l_Subsystem, "eRT.");
	
	sprintf(dst_pathname_cr, "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);
	parameterValStruct_t* val = (parameterValStruct_t*) malloc(sizeof(parameterValStruct_t) * paramCount);

	strcpy(paramName, paramVal[0].name);
	IndexMpa_WEBPAtoCPE(paramName);
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem, /* prefix */
			&ppComponents, &size);
	strcpy(CompName, ppComponents[0]->componentName);
	WalPrint("CompName = %s,paramCount = %d\n", CompName,paramCount);
	if(!strcmp(CompName,"eRT.com.cisco.spvtg.ccsp.wifi")) {
		bRadioRestartEn = TRUE;
	}
	for (cnt = 0; cnt < paramCount; cnt++) {
		strcpy(paramName, paramVal[cnt].name);
		IndexMpa_WEBPAtoCPE(paramName);
		ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
				dst_pathname_cr, paramName, l_Subsystem, /* prefix */
				&ppComponents, &size);
		
		if (ret == CCSP_SUCCESS && size == 1) {
			WalPrint("ppComponents[%d]->componentName = %s\n", cnt,
					ppComponents[0]->componentName);
			if (strcmp(CompName, ppComponents[0]->componentName) == 0) {
				val[cnt].parameterName = malloc(sizeof(char)*512);
				strcpy(val[cnt].parameterName,paramName);
				val[cnt].parameterValue = paramVal[cnt].value;
				WalPrint("val[%d].parameterName = %s\n", cnt,
						val[cnt].parameterName);
				WalPrint("val[%d].parameterValue = %s\n", cnt,
						val[cnt].parameterValue);
				WalPrint("paramVal[%d].type = %d\n", cnt,
						paramVal[cnt].type);
				if (paramVal[cnt].type == 0) {
					val[cnt].type = ccsp_string;
				} else if (paramVal[cnt].type == 1) {
					val[cnt].type = ccsp_int;
				} else if (paramVal[cnt].type == 2) {
					val[cnt].type = ccsp_unsignedInt;
				} else if (paramVal[cnt].type == 3) {
					val[cnt].type = ccsp_boolean;
				} else if (paramVal[cnt].type == 4) {
					val[cnt].type = ccsp_dateTime;
				} else if (paramVal[cnt].type == 5) {
					val[cnt].type = ccsp_base64;
				} else if (paramVal[cnt].type == 6) {
					val[cnt].type = ccsp_long;
				} else if (paramVal[cnt].type == 7) {
					val[cnt].type = ccsp_unsignedLong;
				} else if (paramVal[cnt].type == 8) {
					val[cnt].type = ccsp_float;
				} else if (paramVal[cnt].type == 9) {
					val[cnt].type = ccsp_double;
				} else if (paramVal[cnt].type == 10) {
					val[cnt].type = ccsp_byte;
				} else {
					val[cnt].type = ccsp_none;
				}
			}
			else {
				printf("Error: Parameters does not belong to the same component\n");
				return CCSP_FAILURE;
			} 
		}
		else {
			WalError("Error: Component name is not supported.ret : %d\n", ret);
		}
	}

	if (ret == CCSP_SUCCESS && size == 1) {
		ret = CcspBaseIf_setParameterValues(bus_handle,
				ppComponents[0]->componentName, ppComponents[0]->dbusPath, 0,
				0x0,  //session id and write id
				val, paramCount, TRUE, // no commit
				&faultParam);

		if (ret != CCSP_SUCCESS && faultParam) {
			AnscTraceError(("Error:Failed to SetValue for param '%s'\n", faultParam));
			WalError("Error:Failed to SetValue for param  '%s', ret : %d \n", faultParam, ret);
			if (faultParam) {
				free(faultParam);
				free_componentStruct_t(bus_handle, size, ppComponents);
			}
		}
		else {
			if(bRadioRestartEn) {
				BOOL bRestartRadio1 = FALSE;
				BOOL bRestartRadio2 = FALSE;
				int nreq = 0,x,index;
				int SSID =0,apply_rf;
				parameterValStruct_t *RadApplyParam = NULL;
				parameterValStruct_t val_set[4] = { { "Device.WiFi.Radio.1.X_CISCO_COM_ApplySettingSSID","1", ccsp_int}, 
													{ "Device.WiFi.Radio.1.X_CISCO_COM_ApplySetting", "true", ccsp_boolean},
													{ "Device.WiFi.Radio.2.X_CISCO_COM_ApplySettingSSID","2", ccsp_int},
													{ "Device.WiFi.Radio.2.X_CISCO_COM_ApplySetting", "true", ccsp_boolean}};
				bRadioRestartEn = FALSE;

				for(x =0; x< paramCount; x++) {
					if(!strncmp(paramVal[x].name,"Device.WiFi.Radio.1.",20)) {
						bRestartRadio1 = TRUE; 
					}
					else if(!strncmp(paramVal[x].name,"Device.WiFi.Radio.2.",20)) {
						bRestartRadio2 = TRUE;
					}
					else {
						if((!strncmp(paramVal[x].name,"Device.WiFi.SSID.",17))) {
							sscanf(paramVal[x].name,"Device.WiFi.SSID.%d",&index);
							WalPrint("index = %d\n",index);
							SSID = (1 << ((index)-1));
							apply_rf = (2  - ((index)%2));
							WalPrint("apply_rf = %d\n",apply_rf);
							
							if(apply_rf == 1) {
								bRestartRadio1 = TRUE;
							}
							else if(apply_rf == 2) {
								bRestartRadio2 = TRUE;
							}
						}
						else if(!strncmp(paramVal[x].name,"Device.WiFi.AccessPoint.",24)) {
							sscanf(paramVal[x].name,"Device.WiFi.AccessPoint.%d",&index);
							SSID = (1 << ((index)-1));
							apply_rf = (2  - ((index)%2));
							
							if(apply_rf == 1) {
								bRestartRadio1 = TRUE; 
							}
							else if(apply_rf == 2) {
								bRestartRadio2 = TRUE; 
							}
						}
					}
				}

				if((bRestartRadio1 == TRUE) && (bRestartRadio2 == TRUE)) {
					WalPrint("Need to restart both the Radios\n");
					RadApplyParam = val_set;
					nreq = 4;
				}
				else if(bRestartRadio1) {
					WalPrint("Need to restart Radio 1\n");
					RadApplyParam = val_set;
					nreq = 2;
				}
				else if(bRestartRadio2) {
					WalPrint("Need to restart Radio 2\n");
					RadApplyParam = &val_set[2];
					nreq = 2;
				}

				ret = CcspBaseIf_setParameterValues
				(
					bus_handle, 
					ppComponents[0]->componentName, 
					ppComponents[0]->dbusPath,
					0, 0x0,   /* session id and write id */
					RadApplyParam, 
					nreq, 
					TRUE,   /* no commit */
					&faultParam
				);
				
				if (ret != CCSP_SUCCESS && faultParam) {
					WalError("Failed to Set Apply Settings\n");
					free(faultParam);
				}
			}
			free_componentStruct_t(bus_handle, size, ppComponents);
		}
	} 
	else {
		WalError("Error:Failed to SetValue for param  '%s' ret : %d \n", faultParam, ret);
		if (faultParam) {
			free(faultParam);
			free_componentStruct_t(bus_handle, size, ppComponents);
		}
	}
	for (cnt = 0; cnt < paramCount; cnt++) {
		if (val[cnt].parameterName) {
			free(val[cnt].parameterName);
		}
		val[cnt].parameterName = NULL;
	}
	
	if (val) {
		free(val);
	}
	val = NULL;
	return ret;
}

static int setParamAttributes(const char *pParameterName, const AttrVal *attArr)
{
	char dst_pathname_cr[64] = { 0 };
	extern ANSC_HANDLE bus_handle;
	char l_Subsystem[32] = { 0 };
	int ret;
	int size = 0;
	int notificationType = 0;
	componentStruct_t ** ppComponents = NULL;
	char paramName[100] = { 0 };
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

	if (ret == CCSP_SUCCESS && size == 1) {
		ret = CcspBaseIf_Register_Event(bus_handle, ppComponents[0]->componentName, "parameterValueChangeSignal");

		if (CCSP_SUCCESS != ret) {
			WalError("WebPa: CcspBaseIf_Register_Event failed!!!\n");
		}

		CcspBaseIf_SetCallback2(bus_handle, "parameterValueChangeSignal", ccspWebPaValueChangedCB, NULL);

		attriStruct.parameterName = paramName;
		notificationType = atoi(attArr->value);
		attriStruct.notification = notificationType;

		ret = CcspBaseIf_setParameterAttributes(bus_handle,
				ppComponents[0]->componentName, ppComponents[0]->dbusPath, 0,
				&attriStruct, 1);

		if (CCSP_SUCCESS != ret) {
			CcspTraceDebug(("CcspBaseIf_setParameterAttributes (turn notification on) failed!!!\n"));
		}

		free_componentStruct_t(bus_handle, size, ppComponents);
	}
	else {
		WalError("Error:Failed to SetValue for SetParamAttr ~~~ ret : %d \n", ret);
	}

	return ret;
}

static void IndexMpa_WEBPAtoCPE(char *pParameterName)
{
	int i = 0, j = 0;
	char pDmIntString[WIFI_MAX_STRING_LEN];
	for (i = 0; i < WIFI_PARAM_MAP_SIZE; i++) {
		int dmlNameLen = strlen(CcspDmlName[i]);
		if (strncmp(pParameterName, CcspDmlName[i], dmlNameLen) == 0) {
			char* instNumStart = pParameterName + dmlNameLen;
			char restDmlString[WIFI_MAX_STRING_LEN];
			int instNum;

			if (strlen(pParameterName) < dmlNameLen + 1) {
				// Found match on table, but there is no instance number
				break;
			}
			else {
				if (instNumStart[0] == '.') {
					instNumStart++;
				}
				sscanf(instNumStart, "%d%s", &instNum, restDmlString);

				// Find instance match and translate
				if (i == 0) {
					// For Device.WiFI.Radio.
					j = 0;
				}
				else {
					// For other than Device.WiFI.Radio.
					j = 2;
				}
				for (j; j < WIFI_INDEX_MAP_SIZE; j++) {
					if (IndexMap[j].WebPaInstanceNumber == instNum) {
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

static char* IndexMpa_CPEtoWEBPA(char *pParameterName)
{
	int i = 0, j = 0;
	char *pDmIntString = NULL;
	for (i = 0; i < WIFI_PARAM_MAP_SIZE; i++) {
		int dmlNameLen = strlen(CcspDmlName[i]);
		if (strncmp(pParameterName, CcspDmlName[i], dmlNameLen) == 0) {
			char* instNumStart = pParameterName + dmlNameLen;
			char restDmlString[WIFI_MAX_STRING_LEN];
			int instNum;
			char *temp = NULL;

			if (strlen(pParameterName) < dmlNameLen + 1) {
				// Found match on table, but there is no instance number
				break;
			} else {
				if (instNumStart[0] == '.') {
					instNumStart++;
				}
				sscanf(instNumStart, "%d%s", &instNum, restDmlString);
				// Find instance match and translate
				if (i == 0) {
					// For Device.WiFI.Radio.
					j = 0;
				} else {
					// For other than Device.WiFI.Radio.
					j = 2;
				}
				for (j; j < WIFI_INDEX_MAP_SIZE; j++) {
					if (IndexMap[j].CcspInstanceNumber == instNum) {
						pDmIntString = (char *) malloc(
								sizeof(char) * (dmlNameLen + 250));
						if (pDmIntString) {
							sprintf(pDmIntString, "%s.%d%s", CcspDmlName[i],
									IndexMap[j].WebPaInstanceNumber,
									restDmlString);
							return pDmIntString;
						} else {
							return NULL;
						}

						break;
					}
				}
			}
			break;
		}
	}
	return NULL;
}