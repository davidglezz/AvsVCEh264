/*******************************************************************************
* AvsVCEh264 version 0.2
* Avisynth to h264 encoding using OpenCL API that leverages the video
* compression engine (VCE) on AMD platforms.
* Copyright (C) 2013 David González García <davidgg666@gmail.com>
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
* GNU General Public License for more details. <http://www.gnu.org/licenses/>.
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
#include "configFile.h"
#include "timer.h"
#include "buffer.h"
#include "OVstuff.h"


#include "avisynthUtil.h"


/** Global **/
unsigned int currentFrame = 0;
unsigned int alignedSurfaceWidth = 0;
unsigned int alignedSurfaceHeight = 0;
unsigned int hostPtrSize = 0;


cl_device_id clDeviceID;

// Threads
HANDLE hThreadAvsDec, hThreadEnc, hThreadMonitor;

// Timer
Timer timer;

// Buffer
Buffer* frameBuffer;



DWORD WINAPI threadMonitor(LPVOID id)
{
    fprintf(stderr, "\n");

    unsigned int gpuFreq, size;
    clGetDeviceInfo(clDeviceID, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(unsigned int), &gpuFreq, &size);

	// Show Info
    fprintf(stderr, "Width       %d\n", info->width);
	fprintf(stderr, "Height      %d\n", info->height);
	fprintf(stderr, "Fps         %f\n", info->fps_numerator / (float)info->fps_denominator);
	fprintf(stderr, "Frames      %d\n", info->num_frames);
	fprintf(stderr, "Duration    %d s\n", info->num_frames * info->fps_denominator /  info->fps_numerator);
	fprintf(stderr, "GPU Freq    %6.2f MHz\n", (float)gpuFreq);

	// instatFps
	while (currentFrame == 0)
		Sleep(5);

    while (!GetAsyncKeyState(VK_F8))
    {
    	double time = timer.getInSec();

    	unsigned int percent = currentFrame * 100 / info->num_frames;

		double fps = currentFrame / time;

    	unsigned int remaining_s = time * (double)info->num_frames /
						(double)currentFrame - time;

		unsigned int elapsed_s = time;
    	unsigned int elapsed_h = elapsed_s / 3600;
		elapsed_s %= 3600;
		unsigned int elapsed_m = elapsed_s / 60;
		elapsed_s %= 60;

		unsigned int remaining_h = remaining_s / 3600;
		remaining_s %= 3600;
		unsigned int remaining_m = remaining_s / 60;
		remaining_s %= 60;

        fprintf(stderr, "\r%u%%\t%u/%u\tFps: %3.3f\tElapsed: %u:%02u:%02u\tRem.: %u:%02u:%02u",
				percent, currentFrame, info->num_frames, fps, elapsed_h,
				elapsed_m, elapsed_s, remaining_h, remaining_m, remaining_s);

        Sleep(250);
    }
    fprintf(stderr, "\n");
    return 0;
}


DWORD WINAPI threadAvsDec(LPVOID id)
{
	BYTE pUVrow[info->width];
	for (int f = 0; f < info->num_frames; f++)
	{
		while (BufferIsFull(frameBuffer))
			Sleep(250); // only bad if encodes more than 1000fps

		AVS_VideoFrame *frame = avs_get_frame(clip, currentFrame);

        const BYTE *pYplane = avs_get_read_ptr_p(frame, AVS_PLANAR_Y);
        const BYTE *pUplane = avs_get_read_ptr_p(frame, AVS_PLANAR_U);
		const BYTE *pVplane = avs_get_read_ptr_p(frame, AVS_PLANAR_V);
		BYTE *frameData  = (BYTE*) malloc(hostPtrSize);

		BYTE *pBuf = frameData;

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

		BufferWrite(frameBuffer, (BufferType)frameData);

        // not sure release is needed, but it doesn't cause an error
        avs_release_frame(frame);
	}
	return 0;
}

/*******************************************************************************
 *  @fn     encodeProcess
 *  @brief  Encode an input video file and output encoded H.264 video file
 *  @param[in] encodeHandle : Hanlde for the encoder
 *  @param[out] outFile		: output encoded H.264 video file
 *  @param[in] oveContext   : Hanlde to the encoder context
 *  @param[in] deviceID     : Device on which encoder context to be created
 *  @param[in] pConfig      : OvConfigCtrl
 *  @return bool : true if successful; otherwise false.
 ******************************************************************************/
bool encodeProcess(OVEncodeHandle *encodeHandle, char *outFile,
			OPContextHandle oveContext, unsigned int deviceId, OvConfigCtrl *pConfig)
{
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
    OVE_OUTPUT_DESCRIPTION pTaskDescriptionList[1];

	// Output file handle
    FILE *fw = fopen(outFile, "wb");
    if (fw == NULL)
    {
        printf("Error opening the output file %s\n",outFile);
        return false;
    }


	// Go!
    for (currentFrame = 0; currentFrame < (unsigned)info->num_frames; currentFrame++)
    {
    	if (GetAsyncKeyState(VK_F8))
			break;

		while (BufferIsEmpty(frameBuffer))
			Sleep(250); // only bad if encodes more than 1000fps

        cl_event inMapEvt;
        cl_int status;

        inputSurface = encodeHandle->inputSurfaces[currentFrame % MAX_INPUT_SURFACE];

        // Read the input file frame by frame
        void* mapPtr = clEnqueueMapBuffer(encodeHandle->clCmdQueue, (cl_mem)inputSurface, CL_TRUE,
										CL_MAP_READ | CL_MAP_WRITE, 0, hostPtrSize, 0, NULL, &inMapEvt, &status);

        status = clFlush(encodeHandle->clCmdQueue);
        waitForEvent(inMapEvt);
        status = clReleaseEvent(inMapEvt);

		//Read into the input surface buffer

        BufferType pBuf = 0;
        BufferRead(frameBuffer, &pBuf);
        memcpy((BYTE*)mapPtr, (BYTE*)pBuf, hostPtrSize);
        free(pBuf);

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
        pictureParameter.insertSPS = (OVE_BOOL)(currentFrame == 0)?true:false;
        pictureParameter.pictureStructure = OVE_PICTURE_STRUCTURE_H264_FRAME;
        pictureParameter.forceRefreshMap = (OVE_BOOL)true;
        pictureParameter.forceIMBPeriod = 0;
        pictureParameter.forcePicType = OVE_PICTURE_TYPE_H264_NONE;

        // Encode a single picture.
        // calling VCE for frame encode
        res = OVEncodeTask(encodeHandle->session, numEncodeTaskInputBuffers,
                  encodeTaskInputBufferList, &pictureParameter, &iTaskID,
                  numEventInWaitList, NULL, &eventRunVideoProgram);

        if (!res)
        {
            fprintf(stderr, "OVEncodeTask returned error %d\n", res);
            return false;
        }

        // Wait for Encode session completes
        err = clWaitForEvents(1, (cl_event *)&(eventRunVideoProgram));
        if (err != CL_SUCCESS)
        {
           fprintf(stderr, "clWaitForEvents returned error %d\n", err);
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


	// Free memory resources
    avs_release_clip(clip);
    avs_delete_script_environment(env);
    fclose(fw);
    free(encodeTaskInputBufferList);

    return true;
}



void showHelp()
{
    puts("Help on encoding usages and configurations...\n");
    puts("exe -i input.avs -o output.h264 -c balanced.ini\n");
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

    // Init Buffer
    frameBuffer = newBuffer();

	// Init Avisync
	if (!AVS_Init(input))
        return 1;


    // load configuration
    OvConfigCtrl configCtrl;
    OvConfigCtrl *pConfigCtrl = (OvConfigCtrl*) &configCtrl;
    memset (pConfigCtrl, 0, sizeof (OvConfigCtrl));

    if (!loadConfig(pConfigCtrl, configFile))
        return 1;

	pConfigCtrl->rateControl.encRateControlFrameRateNumerator = info->fps_numerator;
	pConfigCtrl->rateControl.encRateControlFrameRateDenominator = info->fps_denominator;


    // Make sure the surface is byte aligned
    alignedSurfaceWidth = ((info->width + (256 - 1)) & ~(256 - 1));
    alignedSurfaceHeight = (true) ? (info->height + 31) & ~31 : (info->height + 15) & ~15;

	// frame size in memory: NV12 is 3/2
    hostPtrSize = alignedSurfaceHeight * alignedSurfaceWidth * 3 / 2;
    //unsigned int frameSize = info->width * info->height * 3 / 2;

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
    OVEncodeHandle encodeHandle;
    clDeviceID = reinterpret_cast<cl_device_id>(deviceId);

	// hilo de estadisticas
    hThreadMonitor = CreateThread(NULL, 0, threadMonitor, 0, 0, 0);
    SetThreadPriority(hThreadMonitor, THREAD_PRIORITY_IDLE);

    // hilo de lectura
    hThreadAvsDec = CreateThread(NULL, 0, threadAvsDec, 0, 0, 0);

    // Create, initialize & encode a file
    puts("Encoding...\n");
    timer.start();
    status = encodeProcess(&encodeHandle, output, oveContext, deviceId, pConfigCtrl);
    timer.stop();
    if (status == false)
        return 1;

	fprintf(stderr, "\nEncoding complete in %f s\n", timer.getElapsedTime());


	/**/
	TerminateThread(hThreadMonitor, 0);
    CloseHandle(hThreadMonitor);


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

