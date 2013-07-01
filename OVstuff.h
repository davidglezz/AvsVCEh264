/*******************************************************************************
* This file is part of AvsVCEh264.
* Contains declaration & functions for encoding with OpenVideo\OVEncode
*
* Copyright (C) 2013 David González García <davidgg666@gmail.com>
*******************************************************************************/

#include <stdio.h>

// Input surface used for encoder
#define MAX_INPUT_SURFACE	1

typedef struct OVDeviceHandle
{
    ovencode_device_info *deviceInfo;
    unsigned int numDevices;
    cl_platform_id platform;
} OVDeviceHandle;

// Encoder Hanlde for sharing context between create process and destroy
typedef struct OVEncodeHandle
{
    ove_session      session;       // Pointer to encoder session
    OPMemHandle		 inputSurfaces[MAX_INPUT_SURFACE]; // input buffer
    cl_command_queue clCmdQueue;    // command queue
} OVEncodeHandle;



/*******************************************************************************
 *  @fn     getPlatform
 *  @brief  Get platform to run
 *  @param[in] platform : Platform id
 *  @return bool : true if successful; otherwise false.
 ******************************************************************************/
bool getPlatform(cl_platform_id &platform)
{
    cl_uint numPlatforms;
    cl_int err = clGetPlatformIDs(0, NULL, &numPlatforms);
    if (CL_SUCCESS != err)
    {
        fputs("clGetPlatformIDs() failed", stderr);
        return false;
    }

    // If there are platforms, make sure they are AMD.
    if (0 < numPlatforms)
    {
        cl_platform_id* platforms = new cl_platform_id[numPlatforms];
        err = clGetPlatformIDs(numPlatforms, platforms, NULL);
        if (CL_SUCCESS != err)
        {
            fputs("clGetPlatformIDs() failed", stderr);
            delete [] platforms;
            return false;
        }

        // Loop through all the platforms looking for an AMD system.
        for (unsigned int i = 0; i < numPlatforms; ++i)
        {
            char pbuf[100];
            err = clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, sizeof(pbuf), pbuf, NULL);

            // Stop at the first platform that is an AMD system.
            if (strcmp(pbuf, "Advanced Micro Devices, Inc.") == 0)
            {
                platform = platforms[i];
                break;
            }
        }
        delete [] platforms;
    }

    if (NULL == platform)
    {
        puts("Couldn't find AMD platform, cannot proceed.\n");
        return false;
    }

    return true;
}


/*******************************************************************************
 *  @fn     gpuCheck
 *  @brief  Checks for GPU present or not
 *  @param[in] platform : Platform id
 *  @param[out] dType   : Device type returned GPU/CPU
 *  @return bool : true if successful; otherwise false.
 ******************************************************************************/
bool gpuCheck(cl_platform_id platform,cl_device_type* dType)
{
    cl_int err;
    cl_context_properties cps[3] =  { CL_CONTEXT_PLATFORM, (cl_context_properties)platform, 0};

    cl_context context = clCreateContextFromType(cps, (*dType), NULL, NULL, &err);

    if(err == CL_DEVICE_NOT_FOUND)
        *dType = CL_DEVICE_TYPE_CPU;

    clReleaseContext(context);
    return true;
}


/*******************************************************************************
 *  @fn     getDeviceInfo
 *  @brief  returns device information
 *  @param[out] deviceInfo  : Device info
 *  @param[out] numDevices  : Number of devices present
 *  @return bool : true if successful; otherwise false.
 ******************************************************************************/
bool getDeviceInfo(ovencode_device_info **deviceInfo, unsigned int *numDevices)
{
    bool status = OVEncodeGetDeviceInfo(numDevices, 0);

    if(status)
    {
        if(*numDevices == 0)
        {
            puts("No suitable devices found!\n");
            return false;
        }
    }
    else
    {
        puts("OVEncodeGetDeviceInfo failed!\n");
        return false;
    }

    // Get information about each device found
    *deviceInfo = new ovencode_device_info[*numDevices];
    memset(*deviceInfo,0,sizeof(ovencode_device_info)* (*numDevices));
    status = OVEncodeGetDeviceInfo(numDevices, *deviceInfo);
    if(!status)
    {
        puts("OVEncodeGetDeviceInfo failed!\n");
        return false;
    }
    return true;
}


/*******************************************************************************
 *  @fn     getDevice
 *  @brief  returns the platform and devices found
 *  @param[in/out] deviceHandle : Hanlde for the device information
 *  @return bool : true if successful; otherwise false.
 ******************************************************************************/
bool getDevice(OVDeviceHandle *deviceHandle)
{
    bool status;

    // Get the Platform
    deviceHandle->platform = NULL;
    status = getPlatform(deviceHandle->platform);
    if (status == false)
        return false;

    // Check for GPU
    cl_device_type dType = CL_DEVICE_TYPE_GPU;
    status = gpuCheck(deviceHandle->platform, &dType);
    if (status == false)
        return false;

    // Get the number of devices
    deviceHandle->numDevices = 0;
    deviceHandle->deviceInfo = NULL;

    // Memory for deviceInfo gets allocated inside the getDeviceInfo function
    // depending on numDevices. This needs to be freed after the usage.
    status = getDeviceInfo(&deviceHandle->deviceInfo,&deviceHandle->numDevices);

    return status;
}


/*******************************************************************************
 *  @fn     getDeviceCap
 *  @brief  This function returns the device capabilities.
 *  @param[in] oveContext   : Encoder context
 *  @param[in] oveDeviceID  : Device ID
 *  @param[out] encodeCaps : pointer to encoder capabilities structure
 *  @return bool : true if successful; otherwise false.
 ******************************************************************************/
bool getDeviceCap(OPContextHandle oveContext,unsigned int oveDeviceID, OVE_ENCODE_CAPS *encodeCaps)
{
    unsigned int numCaps = 1;

    // initialize the encode capabilities variable
    encodeCaps->EncodeModes          = OVE_AVC_FULL;
    encodeCaps->encode_cap_size      = sizeof(OVE_ENCODE_CAPS);
    encodeCaps->caps.encode_cap_full->max_picture_size_in_MB    = 0;
    encodeCaps->caps.encode_cap_full->min_picture_size_in_MB    = 0;
    encodeCaps->caps.encode_cap_full->num_picture_formats       = 0;
    encodeCaps->caps.encode_cap_full->num_Profile_level         = 0;
    encodeCaps->caps.encode_cap_full->max_bit_rate              = 0;
    encodeCaps->caps.encode_cap_full->min_bit_rate              = 0;
    encodeCaps->caps.encode_cap_full->supported_task_priority   = OVE_ENCODE_TASK_PRIORITY_NONE;

    for (int j = 0; j < OVE_MAX_NUM_PICTURE_FORMATS_H264; j++)
        encodeCaps->caps.encode_cap_full->supported_picture_formats[j] = OVE_PICTURE_FORMAT_NONE;

    for (int j = 0; j < OVE_MAX_NUM_PROFILE_LEVELS_H264; j++)
    {
        encodeCaps->caps.encode_cap_full->supported_profile_level[j].profile = 0;
        encodeCaps->caps.encode_cap_full->supported_profile_level[j].level   = 0;
    }

    // Get the device capabilities & return
    return OVEncodeGetDeviceCap(oveContext, oveDeviceID, encodeCaps->encode_cap_size, &numCaps, encodeCaps);
}


/*******************************************************************************
 *  @fn     encodeCreate
 *  @brief  Creates encoder context
 *  @param[in/out] oveContext   : Hanlde to the encoder context
 *  @param[in] deviceID         : Device on which encoder context to be created
 *  @param[in] deviceHandle     : Hanlde for the device information
 *  @return bool : true if successful; otherwise false.
 ******************************************************************************/
bool encodeCreate(OPContextHandle *oveContext, unsigned int deviceId, OVDeviceHandle *deviceHandle)
{
    cl_device_id clDeviceID;
    bool status;
    cl_int err;
    *oveContext = NULL;

    // 1. Create the CL Context - nothing works without a context handle.
    // Create a variable for the open video encoder device id
    intptr_t properties[] = {CL_CONTEXT_PLATFORM, (cl_context_properties)deviceHandle->platform, 0};

    // Create OpenCL context from device's id
    clDeviceID = reinterpret_cast<cl_device_id>(deviceId);
    *oveContext  = clCreateContext(properties, 1, &clDeviceID, 0, 0, &err);

    if (*oveContext  == (cl_context)0)
    {
        puts("\nCannot create cl_context\n");
        return false;
    }

    if (err != CL_SUCCESS)
    {
        printf("Error in clCreateContext: %d\n", err);
        return false;
    }

    if (deviceId == 0)
    {
        puts("No suitable devices found!\n");
        return false;
    }

    // 2. Read the device capabilities.
    // Device capabilities should be used to validate against the configuration set by the user for the codec
    OVE_ENCODE_CAPS encodeCaps;
    OVE_ENCODE_CAPS_H264 encode_cap_full;
    encodeCaps.caps.encode_cap_full = (OVE_ENCODE_CAPS_H264 *)&encode_cap_full;
    status = getDeviceCap(*oveContext, deviceId, &encodeCaps);

    if(!status)
    {
        puts("OVEncodeGetDeviceCap failed!\n");
        return false;
    }

    return(0);
}


/*******************************************************************************
 *  @fn     setEncodeConfig
 *  @brief  This function sets the encoder configuration by using user supplied configuration information from .cfg file
 *  @param[in] session : Encoder session for which encoder configuration to be set
 *  @param[in] pConfig : pointer to the user configuration from .cfg file
 *  @return bool : true if successful; otherwise false.
 ******************************************************************************/
bool setEncodeConfig(ove_session session, OvConfigCtrl *pConfig)
{
    unsigned int numOfConfigBuffers = 4;
    OVE_CONFIG oveConfig[4];

    // send configuration values for this session
    oveConfig[0].config.pPictureControl     = &(pConfig->pictControl);
    oveConfig[0].configType                 = OVE_CONFIG_TYPE_PICTURE_CONTROL;
    oveConfig[1].config.pRateControl        = &(pConfig->rateControl);
    oveConfig[1].configType                 = OVE_CONFIG_TYPE_RATE_CONTROL;
    oveConfig[2].config.pMotionEstimation   = &(pConfig->meControl);
    oveConfig[2].configType                 = OVE_CONFIG_TYPE_MOTION_ESTIMATION;
    oveConfig[3].config.pRDO                = &(pConfig->rdoControl);
    oveConfig[3].configType                 = OVE_CONFIG_TYPE_RDO;

    if (!(OVresult)OVEncodeSendConfig(session, numOfConfigBuffers, oveConfig))
    {
        printf("OVEncodeSendConfig returned error\n");
        return false;
    }

    // Just verifying that the values have been set in the encoding engine.

    /*OVE_CONFIG_PICTURE_CONTROL      pictureControlConfig;
    OVE_CONFIG_RATE_CONTROL         rateControlConfig;
    OVE_CONFIG_MOTION_ESTIMATION    meControlConfig;
    OVE_CONFIG_RDO                  rdoControlConfig;

    // Get the picture control configuration.
    memset(&pictureControlConfig, 0, sizeof(OVE_CONFIG_PICTURE_CONTROL));
    pictureControlConfig.size = sizeof(OVE_CONFIG_PICTURE_CONTROL);
    if (!(OVresult)OVEncodeGetPictureControlConfig(session, &pictureControlConfig))
    {
        printf("OVEncodeGetPictureControlConfig returned error\n");
        return false;
    }

    // Get the rate control configuration
    memset(&rateControlConfig, 0, sizeof(OVE_CONFIG_RATE_CONTROL));
    rateControlConfig.size = sizeof(OVE_CONFIG_RATE_CONTROL);
    if (!(OVresult)OVEncodeGetRateControlConfig(session, &rateControlConfig))
	{
        printf("OVEncodeGetRateControlConfig returned error\n");
        return false;
    }
    // Get the MotionEstimation configuration
    memset(&meControlConfig, 0, sizeof(OVE_CONFIG_MOTION_ESTIMATION));
    meControlConfig.size = sizeof(OVE_CONFIG_MOTION_ESTIMATION);
    if (!(OVresult)OVEncodeGetMotionEstimationConfig(session, &meControlConfig))
	{
        printf("OVEncodeGetMotionEstimationConfig returned error\n");
        return false;
    }

    // Get the RDO configuration
    memset(&rdoControlConfig, 0, sizeof(OVE_CONFIG_RDO));
    rdoControlConfig.size = sizeof(OVE_CONFIG_RDO);
    if (!OVEncodeGetRDOControlConfig(session, &rdoControlConfig))
	{
        printf("OVEncodeGetRDOControlConfig returned error\n");
        return false;
    }*/

    return true;
}


/*******************************************************************************
 *  @fn     waitForEvent
 *  @brief  This function waits for the event completion
 *  @param[in] inMapEvt : Event for which it has to wait for completion
 *  @return bool : true if successful; otherwise false.
 ******************************************************************************/
void waitForEvent(cl_event inMapEvt)
{
    cl_int status, eventStatus = CL_QUEUED;

    while (eventStatus != CL_COMPLETE)
    {
        status = clGetEventInfo(inMapEvt, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &eventStatus, NULL);
    }
}


/*******************************************************************************
 *  @fn     encodeClose
 *  @brief  This function destroys the resources used by the encoder session
 *  @param[in] encodeHandle : Handle for the encoder context
 *  @return bool : true if successful; otherwise false.
 ******************************************************************************/
bool encodeClose(OVEncodeHandle *encodeHandle)
{
    cl_int err;
    OPMemHandle *inputSurfaces = encodeHandle->inputSurfaces;

    for (int i = 0; i < MAX_INPUT_SURFACE; i++)
    {
        err = clReleaseMemObject((cl_mem)inputSurfaces[i]);
        if (err != CL_SUCCESS)
        {
            printf("clReleaseMemObject returned error %d\n", err);
            return false;
        }
    }

    err = clReleaseCommandQueue(encodeHandle->clCmdQueue);
    if(err != CL_SUCCESS)
    {
        puts("Error releasing Command queue\n");
        return false;
    }

    bool oveErr = OVEncodeDestroySession(encodeHandle->session);

    if (!oveErr)
    {
        puts("Error releasing OVE Session\n");
        return false;
    }

    return true;
}


/******************************************************************************
 *  @fn     encodeDestroy
 *  @brief  Destroy encoder context
 *  @param[in] oveContext : Handle for the encoder context
 *  @return bool : true if successful; otherwise false.
 ******************************************************************************/
bool encodeDestroy(OPContextHandle oveContext)
{
    if ((cl_context)oveContext)
    {
        cl_int err = clReleaseContext((cl_context)oveContext);

        if (err != CL_SUCCESS)
        {
            puts("Error releasing cl context\n");
            return false;
        }
    }
    return true;
}
