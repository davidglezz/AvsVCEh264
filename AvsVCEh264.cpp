/*******************************************************************************
* AvsVCEh264 version 0.1
* Avisynth to h264 encoding using OpenCL API that leverages the video
* compression engine (VCE) on AMD platforms.
* Copyright (C) 2013 David González García <davidgg666@gmail.com>
*
* AvsVCEh264 inspired by avs2pipe by Chris Beswick
* Using video compression engine (VCE) on AMD platforms.
*
********************************************************************************
*
* This file is part of AvsVCEh264.
*
* AvsVCEh264 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* AvsVCEh264 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with AvsVCEh264.  If not, see <http://www.gnu.org/licenses/>.
*
*******************************************************************************/

// Define 64-bit typedefs, depending on the compiler and operating system.
#ifdef __GNUC__
typedef long long           int64;
typedef unsigned long long	uint64;

#else  /* not __GNUC__ */
#ifdef _WIN32
typedef __int64             int64;
typedef unsigned __int64    uint64;

#else  /* not _WIN32 */
#error Unsupported compiler and/or operating system
#endif /* end ifdef _WIN32 */

#endif /* end ifdef __GNUC__ */

#include <windows.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
#include <cl\cl.h>
#include <OpenVideo\OVEncode.h>
#include <OpenVideo\OVEncodeTypes.h>
#include "avisynth_c.h"
#include "configFile.h"
#include "timer.h"


AVS_Clip* avisynth_filter(AVS_Clip *clip, AVS_ScriptEnvironment *env, const char *filter)
{
    AVS_Value val_clip, val_array, val_return;

    val_clip = avs_new_value_clip(clip);
    avs_release_clip(clip);

    val_array = avs_new_value_array(&val_clip, 1);
    val_return = avs_invoke(env, filter, val_array, 0);
    clip = avs_take_clip(val_return, env);

    avs_release_value(val_array);
    avs_release_value(val_clip);
    avs_release_value(val_return);

    return clip;
}

AVS_Clip* avisynth_source(char *file, AVS_ScriptEnvironment *env)
{
    AVS_Clip *clip;
    AVS_Value val_string, val_array, val_return;

    val_string = avs_new_value_string(file);
    val_array = avs_new_value_array(&val_string, 1);
    val_return = avs_invoke(env, "Import", val_array, 0);
    avs_release_value(val_array);
    avs_release_value(val_string);

    if (!avs_is_clip(val_return))
    {
        puts("avs files return value is not a clip.\n");
        return 0;
    }

    clip = avs_take_clip(val_return, env);

    avs_release_value(val_return);

    return clip;
}

/*
********************************************************************************************************
*/

/*******************************************************************************
* @file <OvEncodeSample.cpp>
* @brief Contains declaration & functions for encoding h264 video
*******************************************************************************/

// Input surface used for encoder
#define MAX_INPUT_SURFACE	2

typedef struct OVDeviceHandle
{
    ovencode_device_info *deviceInfo; // Pointer to device info
    unsigned int                numDevices; // Number of devices available
    cl_platform_id        platform;   // Platform
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
    status = getDeviceCap(*oveContext ,deviceId,&encodeCaps);

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
    OVE_CONFIG configBuffers[4];
    OVresult res = 0;

    // send configuration values for this session
    configBuffers[0].config.pPictureControl     = &(pConfig->pictControl);
    configBuffers[0].configType                 = OVE_CONFIG_TYPE_PICTURE_CONTROL;
    configBuffers[1].config.pRateControl        = &(pConfig->rateControl);
    configBuffers[1].configType                 = OVE_CONFIG_TYPE_RATE_CONTROL;
    configBuffers[2].config.pMotionEstimation   = &(pConfig->meControl);
    configBuffers[2].configType                 = OVE_CONFIG_TYPE_MOTION_ESTIMATION;
    configBuffers[3].config.pRDO                = &(pConfig->rdoControl);
    configBuffers[3].configType                 = OVE_CONFIG_TYPE_RDO;

    res = OVEncodeSendConfig (session, numOfConfigBuffers, configBuffers);

    if (!res)
    {
        printf("OVEncodeSendConfig returned error\n");
        return false;
    }

    // Just verifying that the values have been set in the encoding engine.
    OVE_CONFIG_PICTURE_CONTROL      pictureControlConfig;
    OVE_CONFIG_RATE_CONTROL         rateControlConfig;
    OVE_CONFIG_MOTION_ESTIMATION    meControlConfig;
    OVE_CONFIG_RDO                  rdoControlConfig;

    // Get the picture control configuration.
    memset(&pictureControlConfig, 0, sizeof(OVE_CONFIG_PICTURE_CONTROL));
    pictureControlConfig.size = sizeof(OVE_CONFIG_PICTURE_CONTROL);
    res = OVEncodeGetPictureControlConfig(session, &pictureControlConfig);

    // Get the rate control configuration
    memset(&rateControlConfig, 0, sizeof(OVE_CONFIG_RATE_CONTROL));
    rateControlConfig.size = sizeof(OVE_CONFIG_RATE_CONTROL);
    res = OVEncodeGetRateControlConfig(session, &rateControlConfig);

    // Get the MotionEstimation configuration
    memset(&meControlConfig, 0, sizeof(OVE_CONFIG_MOTION_ESTIMATION));
    meControlConfig.size = sizeof(OVE_CONFIG_MOTION_ESTIMATION);
    res = OVEncodeGetMotionEstimationConfig(session, &meControlConfig);

    // Get the RDO configuration
    memset(&rdoControlConfig, 0, sizeof(OVE_CONFIG_RDO));
    rdoControlConfig.size = sizeof(OVE_CONFIG_RDO);
    res = OVEncodeGetRDOControlConfig(session, &rdoControlConfig);

    return res;
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
 *  @fn     encodeProcess
 *  @brief  Encode an input video file and output encoded H.264 video file
 *  @param[in] encodeHandle : Hanlde for the encoder
 *  @param[in] inFile		: input video file to be encoded
 *  @param[out] outFile		: output encoded H.264 video file
 *  @param[in] oveContext   : Hanlde to the encoder context
 *  @param[in] deviceID     : Device on which encoder context to be created
 *  @param[in] pConfig      : OvConfigCtrl
 *  @return bool : true if successful; otherwise false.
 ******************************************************************************/
bool encodeProcess(OVEncodeHandle *encodeHandle, char *inFile, char *outFile,
			OPContextHandle oveContext, unsigned int deviceId, OvConfigCtrl *pConfig)
{
	cl_device_id clDeviceID = reinterpret_cast<cl_device_id>(deviceId);
	AVS_ScriptEnvironment *env = avs_create_script_environment(AVISYNTH_INTERFACE_VERSION);
    AVS_Clip *clip = avisynth_source(inFile, env);

    const AVS_VideoInfo *info = avs_get_video_info(clip);

    if (!avs_has_video(info))
    {
        fprintf(stderr, "Clip has no video.\n");
        return false;
    }

    pConfig->rateControl.encRateControlFrameRateNumerator = info->fps_numerator;
	pConfig->rateControl.encRateControlFrameRateDenominator = info->fps_denominator;

	unsigned int gpuFreq, size;
    clGetDeviceInfo(clDeviceID, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(unsigned int), &gpuFreq, &size);

	// Show Info
    fprintf(stderr, "Width       %d\n", info->width);
	fprintf(stderr, "Height      %d\n", info->height);
	fprintf(stderr, "Fps         %f\n", info->fps_numerator / (float)info->fps_denominator);
	fprintf(stderr, "Frames      %d\n", info->num_frames);
	fprintf(stderr, "Duration    %d s\n", info->num_frames * info->fps_denominator /  info->fps_numerator);
	fprintf(stderr, "GPU Freq    %6.2f MHz\n", (float)gpuFreq);

    // ensure video is yv12
    if (avs_has_video(info) && !avs_is_yv12(info))
    {
        fprintf(stderr, "Converting video to yv12.\n");
        clip = avisynth_filter(clip, env, "ConvertToYV12");
        info = avs_get_video_info(clip);

        if (!avs_is_yv12(info))
        {
        	fprintf(stderr, "Failed to convert video to yv12.\n");
            return false;
        }
    }

	// Vars
	//unsigned int frameSize = info->width * info->height * 3 / 2;
	unsigned int framecount = info->num_frames;

    // Make sure the surface is byte aligned
    unsigned int alignedSurfaceWidth = ((info->width + (256 - 1)) & ~(256 - 1));
    unsigned int alignedSurfaceHeight = (true) ? (info->height + 31) & ~31 : (info->height + 15) & ~15;

	// NV12 is 3/2
    int hostPtrSize = alignedSurfaceHeight * alignedSurfaceWidth * 3/2;

    cl_int err;
    unsigned int numEventInWaitList = 0;
    OPMemHandle inputSurface;
    OVresult res = 0;
    OPEventHandle eventRunVideoProgram;

    // Initilizes encoder session & buffers
    // Create an OVE Session (Platform context, id, mode, profile, format, ...)
    // encode task priority. FOR POSSIBLY LOW LATENCY OVE_ENCODE_TASK_PRIORITY_LEVEL2 */
    encodeHandle->session = OVEncodeCreateSession(oveContext, deviceId,
                                pConfig->encodeMode, pConfig->profileLevel,
                                pConfig->pictFormat, info->width,
                                info->height, pConfig->priority);

    if (encodeHandle->session == NULL)
    {
        puts("OVEncodeCreateSession failed.\n");
        return false;
    }

    // Configure the encoding engine based upon the config file specifications
    res = setEncodeConfig(encodeHandle->session, pConfig);
    if (!res)
    {
        puts("OVEncodeSendConfig returned error\n");
        return false;
    }

    // Create a command queue
    encodeHandle->clCmdQueue = clCreateCommandQueue((cl_context)oveContext, clDeviceID, 0, &err);
    if(err != CL_SUCCESS)
    {
        printf("Create command queue failed! Error :%d\n", err);
        return false;
    }

    for(int i = 0; i < MAX_INPUT_SURFACE; i++)
    {
        encodeHandle->inputSurfaces[i] = clCreateBuffer((cl_context)oveContext, CL_MEM_READ_WRITE, hostPtrSize, NULL, &err);

        if (err != CL_SUCCESS)
        {
            printf("clCreateBuffer returned error %d\n", err);
            return false;
        }
    }


    // Setup the picture parameters
    OVE_ENCODE_PARAMETERS_H264 pictureParameter;
    unsigned int numEncodeTaskInputBuffers = 1;
    OVE_INPUT_DESCRIPTION *encodeTaskInputBufferList
			= (OVE_INPUT_DESCRIPTION *) malloc(sizeof(OVE_INPUT_DESCRIPTION) *
				numEncodeTaskInputBuffers);

    // For the Query Output
    unsigned int iTaskID;
    unsigned int numTaskDescriptionsRequested = 1;
    unsigned int numTaskDescriptionsReturned = 0;
    unsigned int framenum = 0;
    OVE_OUTPUT_DESCRIPTION pTaskDescriptionList[1];

	// Output file handle
    FILE *fw = fopen(outFile, "wb");
    if (fw == NULL)
    {
        printf("Error opening the output file %s\n",outFile);
        return false;
    }

	// Go!
    for (framenum = 0; framenum < framecount; framenum++)
    {
        cl_event inMapEvt;
        cl_int status;

        inputSurface = encodeHandle->inputSurfaces[framenum % MAX_INPUT_SURFACE];

        // Read the input file frame by frame
        void* mapPtr = clEnqueueMapBuffer(encodeHandle->clCmdQueue, (cl_mem)inputSurface, CL_TRUE,
										CL_MAP_READ | CL_MAP_WRITE, 0, hostPtrSize, 0, NULL, &inMapEvt, &status);

        status = clFlush(encodeHandle->clCmdQueue);
        waitForEvent(inMapEvt);
        status = clReleaseEvent(inMapEvt);

		//Read into the input surface buffer
        BYTE *pBuf = (BYTE*)mapPtr;
        AVS_VideoFrame *frame = avs_get_frame(clip, framenum);

        const BYTE *pYplane = avs_get_read_ptr_p(frame, AVS_PLANAR_Y);
        const BYTE *pUplane = avs_get_read_ptr_p(frame, AVS_PLANAR_U);
		const BYTE *pVplane = avs_get_read_ptr_p(frame, AVS_PLANAR_V);
		BYTE *pUVrow  = (unsigned char*) malloc(info->width);

        // Y plane
        unsigned int pitch = avs_get_pitch_p(frame, AVS_PLANAR_Y);
		for (int h = 0; h < info->height; h++)
		{
			memcpy(pBuf, pYplane, info->width);
			pBuf += alignedSurfaceWidth;
			pYplane += pitch;
		}

		// UV planes
		unsigned int uiHalfHeight = info->height >> 1;
		unsigned int uiHalfWidth  = info->width >> 1; //chromaWidth
		unsigned int pos = 0;
		for (unsigned int h = 0; h < uiHalfHeight; h++)
		{
			for (unsigned int i = 0; i < uiHalfWidth; ++i)
			{
				pUVrow[i*2]     = pUplane[pos + i];
				pUVrow[i*2 + 1] = pVplane[pos + i];
			}
			memcpy(pBuf, pUVrow, info->width);
			pBuf += alignedSurfaceWidth;
			pos += uiHalfWidth;
		}

        // not sure release is needed, but it doesn't cause an error
        avs_release_frame(frame);

        fprintf(stderr, "\r%d%% ", 100 * framenum / framecount);

        // ------- END READIN FRAME ---------
        cl_event unmapEvent;
        status = clEnqueueUnmapMemObject(encodeHandle->clCmdQueue, (cl_mem)inputSurface, mapPtr, 0, NULL, &unmapEvent);
        status = clFlush(encodeHandle->clCmdQueue);
        waitForEvent(unmapEvent);
        status = clReleaseEvent(unmapEvent);

        // use the input surface buffer as our Picture
        encodeTaskInputBufferList[0].bufferType = OVE_BUFFER_TYPE_PICTURE;
        encodeTaskInputBufferList[0].buffer.pPicture =  (OVE_SURFACE_HANDLE) inputSurface;

        // Setup the picture parameters
        memset(&pictureParameter, 0, sizeof(OVE_ENCODE_PARAMETERS_H264));
        pictureParameter.size = sizeof(OVE_ENCODE_PARAMETERS_H264);
        pictureParameter.flags.value = 0;
        pictureParameter.flags.flags.reserved = 0;
        pictureParameter.insertSPS = (OVE_BOOL)(framenum == 0)?true:false;
        pictureParameter.pictureStructure = OVE_PICTURE_STRUCTURE_H264_FRAME;
        pictureParameter.forceRefreshMap = (OVE_BOOL)true;
        pictureParameter.forceIMBPeriod = 0;
        pictureParameter.forcePicType = OVE_PICTURE_TYPE_H264_NONE;

        // Encode a single picture.
        // calling VCE for frame encode
        res = OVEncodeTask(encodeHandle->session, numEncodeTaskInputBuffers,
                  encodeTaskInputBufferList, &pictureParameter, &iTaskID,
                  numEventInWaitList, NULL,
                  &eventRunVideoProgram);

        if (!res)
        {
            printf("OVEncodeTask returned error %d\n", res);
            return false;
        }

        // Wait for Encode session completes
        err = clWaitForEvents(1, (cl_event *)&(eventRunVideoProgram));
        if (err != CL_SUCCESS)
        {
            printf("clWaitForEvents returned error %d\n", err);
            return false;
        }

        // Query output
        numTaskDescriptionsReturned = 0;
        memset(pTaskDescriptionList, 0, sizeof(OVE_OUTPUT_DESCRIPTION)*numTaskDescriptionsRequested);
        pTaskDescriptionList[0].size = sizeof(OVE_OUTPUT_DESCRIPTION);

        do
        {
            res = OVEncodeQueryTaskDescription(encodeHandle->session, numTaskDescriptionsRequested,
                                               &numTaskDescriptionsReturned, pTaskDescriptionList);

            if (!res)
            {
                fprintf(stderr, "OVEncodeQueryTaskDescription returned error %d\n", err);
                return false;
            }
        }
        while (pTaskDescriptionList->status == OVE_TASK_STATUS_NONE);

        // Write compressed frame to the output file
        for(unsigned int i = 0; i < numTaskDescriptionsReturned; i++)
        {
            if (pTaskDescriptionList[i].status == OVE_TASK_STATUS_COMPLETE &&
                    pTaskDescriptionList[i].size_of_bitstream_data > 0)
            {
                // Write output data
                fwrite(pTaskDescriptionList[i].bitstream_data, 1,
                       pTaskDescriptionList[i].size_of_bitstream_data, fw);

                res = OVEncodeReleaseTask(encodeHandle->session, pTaskDescriptionList[i].taskID);
            }
        }

        if (eventRunVideoProgram)
            clReleaseEvent((cl_event) eventRunVideoProgram);
    }


    fprintf(stderr, "\r%d%% ", 100 * framenum / framecount);

	// Free memory resources
    avs_release_clip(clip);
    avs_delete_script_environment(env);
    fclose(fw);
    free(encodeTaskInputBufferList);

    return true;
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

/*
********************************************************************************************************
*/

void showHelp()
{
    puts("Help on encoding usages and configurations...\n");
    puts("exe -i input_Yuv_file.yuv -o output_h264_encoded_file.h264 -c balanced.cfg.cfg\n");
}

int GetWindowsVersion()
{
	// Find the version of Windows
    OSVERSIONINFO vInfo;
    memset(&vInfo, 0, sizeof(vInfo));
    vInfo.dwOSVersionInfoSize = sizeof(vInfo);

	return GetVersionEx(&vInfo) ? vInfo.dwMajorVersion : 0;
}

int main(int argc, char* argv[])
{
    char input[255] = {0};
    char output[255] = {0};
    char configFile[255] = {0};

	// Currently the OpenEncode support is only for vista and w7
    if(GetWindowsVersion() < 6)
    {
        puts("Error : Unsupported OS! Vista/Win7 required.\n");
        return 1;
    }

    // Helps on command line configuration usage cases
    if (argc < 2)
    {
        showHelp();
        return 1;
    }

    // processing the command line and configuration file
    int argCheck = 0;
    for (int i = 0; i < argc; i++)
    {
        if (strncmp(argv[i], "-h", 2) == 0)
        {
            showHelp();
            return 1;
        }

        // processing working directory and input file
        if (strncmp (argv[i], "-i", 2) == 0)
        {
            strcat(input, argv[i+1]);
            argCheck++;
        }

        // processing working directory and output file
        if (strncmp (argv[i], "-o", 2) == 0 )
        {
            strcat(output, argv[i+1]);
            argCheck++;
        }

        if (strncmp(argv[i], "-c", 2) == 0 )
        {
            strcat(configFile, argv[i+1]);
            argCheck++;
        }
    }

    if(argCheck != 3)
    {
        showHelp();
        return 1;
    }

    // get the pointer to configuration controls
    OvConfigCtrl configCtrl;
    OvConfigCtrl *pConfigCtrl = (OvConfigCtrl*) &configCtrl;
    memset (pConfigCtrl, 0, sizeof (OvConfigCtrl));

    if (!loadConfig(pConfigCtrl, configFile))
        return 1;

    // Query for the device information:
    // This function fills the device handle with number of devices available and devices ids.
    OVDeviceHandle deviceHandle;
    puts("Initializing Encoder...\n");
    bool status = getDevice(&deviceHandle);
    if (status == false)
        return 1;

    // Check deviceHandle.numDevices for number of devices and choose the device on
    // which user wants to create the encoder. In this case device 0 is choosen.
    unsigned int deviceId = deviceHandle.deviceInfo[0].device_id;

    // Create the encoder context on the device specified by deviceID
    OPContextHandle oveContext;
    encodeCreate(&oveContext, deviceId, &deviceHandle);

    Timer timer;
    OVEncodeHandle encodeHandle;

    // Create, initialize & encode a file
    puts("Encoding...\n");
    timer.start();
    status = encodeProcess(&encodeHandle, input, output, oveContext, deviceId, pConfigCtrl);
    timer.stop();
    if (status == false)
        return 1;

	fprintf(stderr, "Encoding complete in %f s\n", timer.getElapsedTime());

    // Free the resources used by the encoder session
    status = encodeClose(&encodeHandle);
    if (status == false)
        return 1;

    // Destroy the encoder context
    status = encodeDestroy(oveContext);

    // Free memory used for deviceInfo.
    delete [] deviceHandle.deviceInfo;

    if (status == false)
        return 1;

    // All done
    fprintf(stderr, "Output written to %s \n", output);
    return 0;
}

