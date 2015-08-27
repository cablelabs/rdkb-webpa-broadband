/**
 * @file wal.c
 *
 * @description This file describes the Webpa Abstraction Layer
 */
#include "ssp_global.h"
#include "stdlib.h"
#include "ccsp_dm_api.h"
#include "wal.h"


static WAL_STATUS mappingStatus(int ret);
static void CcspWebPaValueChangedCB(ParamNotify *paramNotify, int size,void* user_data);
static int GetParamVal(char *pParameterName, ParamVal ***parametervalArr,int *TotalParams);
static int getParamAttributes(char *pParameterName, AttrVal ***attr, int *TotalParams);
static int setParamValues(ParamVal paramVal);
static int setParamAttributes(char *pParameterName, AttrVal *attArr);

#define CCSP_ERR_WILDCARD_NOT_SUPPORTED 110
void (*fp_stack)(ParamNotify*);
extern ANSC_HANDLE bus_handle;

/*
* @brief WAL_STATUS mappingStatus Defines WAL status values from corresponding ccsp values
* @param[in] ret ccsp status values from stack
*/
static WAL_STATUS mappingStatus(int ret) {

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
	}
}

static void CcspWebPaValueChangedCB(ParamNotify *paramNotify, int size,
		void* user_data) {
	(*fp_stack)(paramNotify);
}

WAL_STATUS RegisterNotifyCB(notifyCB cb) {

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
void getValues(const char *paramName[], int paramCount, ParamVal ***paramValArr,
		int *retValCount, WAL_STATUS *retStatus) {

	int cnt = 0;
	int i = 0;
	int ret = 0;
	for (cnt = 0; cnt < paramCount; cnt++) {
		ret = GetParamVal(paramName[cnt], &paramValArr[cnt], &retValCount[cnt]);
		retStatus[cnt] = mappingStatus(ret);
		printf("Parameter Name: %s, Parameter Value return: %d\n",paramName[cnt],retStatus[cnt]);
	}

}

/**
 * @brief GetParamVal Returns the parameter Values from stack for GET request
 *
 * @param[in] paramName parameter Name
 * @param[in] paramCount Number of parameters
 * @param[in] paramValArr parameter value Array
 * @param[out] TotalParams Number of parameters returned from stack
 */
static int GetParamVal(char *pParameterName, ParamVal ***parametervalArr,
		int *TotalParams) {

	char dst_pathname_cr[64] = { 0 };
	char l_Subsystem[32] = { 0 };
	int ret;
	int size = 0;
	componentStruct_t ** ppComponents = NULL;
	char paramName[100] = { 0 };
	char *p = &paramName;
	parameterValStruct_t **parameterval = NULL;
	strcpy(l_Subsystem, "eRT.");
	strcpy(paramName, pParameterName);
	sprintf(dst_pathname_cr, "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);

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
			parametervalError[0]->parameterName = pParameterName;
			parametervalError[0]->type = ccsp_string;
			*parametervalArr = parametervalError;
			*TotalParams = 1;
			printf("Error1:Failed to GetValue for param %s ~~~~ ret: %d\n",
					paramName, ret);

		} else {
			*TotalParams = val_size;
			int i;
			parametervalArr[0] = (ParamVal **) malloc(
					sizeof(ParamVal*) * val_size);
			for (i = 0; i < val_size; i++) {
				parametervalArr[0][i] = parameterval[i];
				printf("success: %s %s %d \n",parametervalArr[0][i]->name,parametervalArr[0][i]->value,parametervalArr[0][i]->type);
			}

		}
	} else {
		parametervalError[0] = (parameterValStruct_t *) malloc(
				sizeof(parameterValStruct_t));
		parametervalError[0]->parameterValue = "ERROR";
		parametervalError[0]->parameterName = pParameterName;
		parametervalError[0]->type = ccsp_string;
		*parametervalArr = parametervalError;
		*TotalParams = 1;
		printf("Error3:Failed to GetValue for param %s ~~~~ ret: %d\n",
				paramName, ret);
	}
	return ret;
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
void getAttributes(const char *paramName[], int paramCount, AttrVal ***attr,
		int *retAttrCount, WAL_STATUS *retStatus) {

	int cnt = 0;
	int ret = 0;

	for (cnt = 0; cnt < paramCount; cnt++) {
		ret = getParamAttributes(paramName[cnt], &attr[cnt], &retAttrCount[cnt]);
		retStatus[cnt] = mappingStatus(ret);
		printf("Parameter Name: %s, Parameter Attributes return: %d\n",paramName[cnt],retStatus[cnt]);
	}
}

static int getParamAttributes(char *pParameterName, AttrVal ***attr, int *TotalParams) {

	char dst_pathname_cr[64] = { 0 };
	char l_Subsystem[32] = { 0 };
	int ret;
	int size = 0;
	componentStruct_t ** ppComponents = NULL;
	char paramName[100] = { 0 };
	int sizeAttrArr = 0;
	char *p = &paramName;
	parameterAttributeStruct_t** ppAttrArray = NULL;
	strcpy(l_Subsystem, "eRT.");
	strcpy(paramName, pParameterName);
	sprintf(dst_pathname_cr, "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);

	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem,  //prefix
			&ppComponents, &size);

	if (ret == CCSP_SUCCESS && size == 1) {
		char *parameterNames[1];
		int val_size = 0;
		parameterNames[0] = p;

		ret = CcspBaseIf_getParameterAttributes(bus_handle,
				ppComponents[0]->componentName, ppComponents[0]->dbusPath,
				parameterNames, 1, &sizeAttrArr, &ppAttrArray);

		if (CCSP_SUCCESS != ret) {
			attr[0] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
			attr[0][0] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
			attr[0][0]->name = (char *) malloc(sizeof(char) * 100);
			attr[0][0]->value = (char *) malloc(sizeof(char) * 100);
			sprintf(attr[0][0]->value, "%d", -1);
			strcpy(attr[0][0]->name, pParameterName);
			attr[0][0]->type = WAL_INT;
			*TotalParams = 1;
			CcspTraceDebug(
					("CcspBaseIf_setParameterAttributes (turn notification on) failed!!!\n"));
			printf("Error:Failed to GetValue for GetParamAttr ~~~ ret : %d \n",
					ret);
		} else {
			int x = 0;
			*TotalParams = sizeAttrArr;
			attr[0] = (AttrVal *) malloc(sizeof(AttrVal) * sizeAttrArr);
			for (x = 0; x < sizeAttrArr; x++) {
				attr[0][x] = (AttrVal *) malloc(sizeof(AttrVal) * 1);
				attr[0][x]->name = (char *) malloc(sizeof(char) * 100);
				attr[0][x]->value = (char *) malloc(sizeof(char) * 100);

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

		sprintf(attr[0][0]->value, "%d", -1);
		strcpy(attr[0][0]->name, pParameterName);
		attr[0][0]->type = WAL_INT;
		*TotalParams = 1;
		printf("~~~~ Error:Failed to GetValue for GetParamAttr ~~~ ret : %d \n",
				ret);
	}
	return ret;
}

/**
 * @brief setValues Returns the parameter Names from stack for GET request
 *
 * @param[in] paramName parameter Name
 * @param[in] paramCount Number of parameters
 * @param[out] retStatus Returns parameter Value from the stack
 */
void setValues(const ParamVal paramVal[], int paramCount, WAL_STATUS *retStatus) {
	int cnt = 0;
	int ret = 0;
	for (cnt = 0; cnt < paramCount; cnt++) {
		ret = setParamValues(paramVal[cnt]);
		retStatus[cnt] = mappingStatus(ret);
	}
}

static int setParamValues(ParamVal paramVal) {
	char* faultParam = NULL;
	int nResult = CCSP_SUCCESS;
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

	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem, /* prefix */
			&ppComponents, &size);

	if (ret == CCSP_SUCCESS && size == 1) {

		parameterValStruct_t val[1] = { { "Device.LogAgent.WifiLogMsg", "hello",
				ccsp_string } };

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
			AnscTraceError(
					("Error:Failed to SetValue for param '%s'\n", faultParam));
			printf(
					"~~~~ Error:Failed to SetValue for param  '%s' ~~~~ ret : %d \n",
					faultParam, ret);
		}

	} else {
		printf("~~~~ Error:Failed to SetValue for param  '%s' ~~~~ ret : %d \n",
				faultParam, ret);
	}
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////

void setAttributes(const char *paramName[], int paramCount,
		const AttrVal *attArr[], WAL_STATUS *retStatus) {
	int cnt = 0;
	int ret = 0;

	for (cnt = 0; cnt < paramCount; cnt++) {
		ret = setParamAttributes(paramName[cnt], attArr[cnt]);
		retStatus[cnt] = mappingStatus(ret);
	}

}

static int setParamAttributes(char *pParameterName, AttrVal *attArr) {
	char* faultParam = NULL;
	int nResult = CCSP_SUCCESS;
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

	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem, /* prefix */
			&ppComponents, &size);

	if (ret == CCSP_SUCCESS && size == 1) {
		ret = CcspBaseIf_Register_Event(bus_handle,
				ppComponents[0]->componentName, "parameterValueChangeSignal");

		if (CCSP_SUCCESS != ret) {
			printf("WebPa: CcspBaseIf_Register_Event failed!!!\n");
		}

		CcspBaseIf_SetCallback2(bus_handle, "parameterValueChangeSignal",
				CcspWebPaValueChangedCB, NULL);

		attriStruct.parameterName = paramName;
		notificationType = atoi(attArr->value);
		attriStruct.notification = notificationType;

		ret = CcspBaseIf_setParameterAttributes(bus_handle,
				ppComponents[0]->componentName, ppComponents[0]->dbusPath, 0,
				&attriStruct, 1);

		if (CCSP_SUCCESS != ret) {
			CcspTraceDebug(
					("CcspBaseIf_setParameterAttributes (turn notification on) failed!!!\n"));
		}

		free_componentStruct_t(bus_handle, size, ppComponents);
	} else {
		printf("~~~~ Error:Failed to SetValue for SetParamAttr ~~~ ret : %d \n",
				ret);
	}

	return ret;
}
