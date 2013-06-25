/*******************************************************************************
* This file is part of AvsVCEh264.
* Contains declaration & functions for reading the configuration file
*
* Copyright (C) 2013 David González García <davidgg666@gmail.com>
*******************************************************************************/

#include "ini.h"

typedef struct _OVConfigCtrl
{
    unsigned int                    height;
    unsigned int                    width;
    OVE_ENCODE_MODE                 encodeMode;
    OVE_PROFILE_LEVEL               profileLevel; // Profile Level
    OVE_PICTURE_FORMAT              pictFormat;   // Profile format
    OVE_ENCODE_TASK_PRIORITY        priority;     // priority settings
    OVE_CONFIG_PICTURE_CONTROL      pictControl;  // Picture control
    OVE_CONFIG_RATE_CONTROL         rateControl;  // Rate contorl config
    OVE_CONFIG_MOTION_ESTIMATION    meControl;    // Motion Estimation settings
    OVE_CONFIG_RDO                  rdoControl;   // Rate distorsion optimization control
} OvConfigCtrl, far *pConfig;


static int handler(void* user, const char* section, const char* name, const char* value)
{
    OvConfigCtrl *pConfig = (OvConfigCtrl*)user;

    // value is always int
    int iVal = atoi(value);

    #define IS_SECTION(s) strcmp(section, s) == 0
    #define IS_NAME(n) strcmp(name, n) == 0

    if (IS_SECTION("general"))
    {
		if (IS_NAME("encodeMode"))
		{
			pConfig->encodeMode = (OVE_ENCODE_MODE) iVal;
		}
		else if (IS_NAME("level"))
		{
			pConfig->profileLevel.level = iVal;
		}
		else if (IS_NAME("profile"))
		{
			pConfig->profileLevel.profile = iVal;
		}
		else if (IS_NAME("pictureFormat"))
		{
			pConfig->pictFormat	= (OVE_PICTURE_FORMAT) iVal;
		}
		else if (IS_NAME("requestedPriority"))
		{
			pConfig->priority = (OVE_ENCODE_TASK_PRIORITY) iVal;
		}
		else
		{
			// Unknown
		}
    }
    else if (IS_SECTION("picture"))
    {
		if (IS_NAME("useConstrainedIntraPred"))
		{
			pConfig->pictControl.useConstrainedIntraPred = iVal;
		}
		else if (IS_NAME("CABACEnable"))
		{
			pConfig->pictControl.cabacEnable = iVal;
		}
		else if (IS_NAME("CABACIDC"))
		{
			pConfig->pictControl.cabacIDC = iVal;
		}
		else if (IS_NAME("loopFilterDisable"))
		{
			pConfig->pictControl.loopFilterDisable = iVal;
		}
		else if (IS_NAME("encLFBetaOffset"))
		{
			pConfig->pictControl.encLFBetaOffset = iVal;
		}
		else if (IS_NAME("encLFAlphaC0Offset"))
		{
			pConfig->pictControl.encLFAlphaC0Offset = iVal;
		}
		else if (IS_NAME("encIDRPeriod"))
		{
			pConfig->pictControl.encIDRPeriod = iVal;
		}
		else if (IS_NAME("encIPicPeriod"))
		{
			pConfig->pictControl.encIPicPeriod = iVal;
		}
		else if (IS_NAME("encHeaderInsertionSpacing"))
		{
			pConfig->pictControl.encHeaderInsertionSpacing = iVal;
		}
		else if (IS_NAME("encCropLeftOffset"))
		{
			pConfig->pictControl.encCropLeftOffset = iVal;
		}
		else if (IS_NAME("encCropRightOffset"))
		{
			pConfig->pictControl.encCropRightOffset = iVal;
		}
		else if (IS_NAME("encCropTopOffset"))
		{
			pConfig->pictControl.encCropTopOffset = iVal;
		}
		else if (IS_NAME("encCropBottomOffset"))
		{
			pConfig->pictControl.encCropBottomOffset = iVal;
		}
		else if (IS_NAME("encNumMBsPerSlice"))
		{
			pConfig->pictControl.encNumMBsPerSlice = iVal;
		}
		else if (IS_NAME("encNumSlicesPerFrame"))
		{
			pConfig->pictControl.encNumSlicesPerFrame = iVal;
		}
		else if (IS_NAME("encForceIntraRefresh"))
		{
			pConfig->pictControl.encForceIntraRefresh = iVal;
		}
		else if (IS_NAME("encForceIMBPeriod"))
		{
			pConfig->pictControl.encForceIMBPeriod = iVal;
		}
		else if (IS_NAME("encInsertVUIParam"))
		{
			pConfig->pictControl.encInsertVUIParam = iVal;
		}
		else if (IS_NAME("encInsertSEIMsg"))
		{
			pConfig->pictControl.encInsertSEIMsg = iVal;
		}
		else
		{
			// Unknown
		}
    }
    else if (IS_SECTION("rate"))
    {
		if (IS_NAME("encRateControlMethod"))
		{
			pConfig->rateControl.encRateControlMethod = iVal;
		}
		else if (IS_NAME("encRateControlTargetBitRate"))
		{
			pConfig->rateControl.encRateControlTargetBitRate = iVal;
		}
		else if (IS_NAME("encRateControlPeakBitRate"))
		{
			pConfig->rateControl.encRateControlPeakBitRate = iVal;
		}
		/*else if (IS_NAME("encRateControlFrameRateNumerator"))
		{
			pConfig->rateControl.encRateControlFrameRateNumerator = iVal;
		}
		else if (IS_NAME("encRateControlFrameRateDenominator"))
		{
			pConfig->rateControl.encRateControlFrameRateDenominator = iVal;
		}*/
		else if (IS_NAME("encGOPSize"))
		{
			pConfig->rateControl.encGOPSize = iVal;
		}
		else if (IS_NAME("encRCOptions"))
		{
			pConfig->rateControl.encRCOptions = iVal;
		}
		else if (IS_NAME("encVBVBufferSize"))
		{
			pConfig->rateControl.encVBVBufferSize = iVal;
		}
		else if (IS_NAME("encQP_I"))
		{
			pConfig->rateControl.encQP_I = iVal;
		}
		else if (IS_NAME("encQP_P"))
		{
			pConfig->rateControl.encQP_P = iVal;
		}
		else if (IS_NAME("encQP_B"))
		{
			pConfig->rateControl.encQP_B = iVal;
		}
		else
		{
			// Unknown
		}
    }
    else if (IS_SECTION("ME"))
    {
		if (IS_NAME("IMEDecimationSearch"))
		{
			pConfig->meControl.imeDecimationSearch = iVal;
		}
		else if (IS_NAME("motionEstHalfPixel"))
		{
			pConfig->meControl.motionEstHalfPixel = iVal;
		}
		else if (IS_NAME("motionEstQuarterPixel"))
		{
			pConfig->meControl.motionEstQuarterPixel = iVal;
		}
		else if (IS_NAME("disableFavorPMVPoint"))
		{
			pConfig->meControl.disableFavorPMVPoint = iVal;
		}
		else if (IS_NAME("forceZeroPointCenter"))
		{
			pConfig->meControl.forceZeroPointCenter = iVal;
		}
		else if (IS_NAME("LSMVert"))
		{
			pConfig->meControl.lsmVert = iVal;
		}
		else if (IS_NAME("encSearchRangeX"))
		{
			pConfig->meControl.encSearchRangeX = iVal;
		}
		else if (IS_NAME("encSearchRangeY"))
		{
			pConfig->meControl.encSearchRangeY = iVal;
		}
		else if (IS_NAME("encSearch1RangeX"))
		{
			pConfig->meControl.encSearch1RangeX = iVal;
		}
		else if (IS_NAME("encSearch1RangeY"))
		{
			pConfig->meControl.encSearch1RangeY = iVal;
		}
		else if (IS_NAME("disable16x16Frame1"))
		{
			pConfig->meControl.disable16x16Frame1 = iVal;
		}
		else if (IS_NAME("disableSATD"))
		{
			pConfig->meControl.disableSATD = iVal;
		}
		else if (IS_NAME("enableAMD"))
		{
			pConfig->meControl.enableAMD = iVal;
		}
		else if (IS_NAME("encDisableSubMode"))
		{
			pConfig->meControl.encDisableSubMode = iVal;
		}
		else if (IS_NAME("encIMESkipX"))
		{
			pConfig->meControl.encIMESkipX = iVal;
		}
		else if (IS_NAME("encIMESkipY"))
		{
			pConfig->meControl.encIMESkipY = iVal;
		}
		else if (IS_NAME("encEnImeOverwDisSubm"))
		{
			pConfig->meControl.encEnImeOverwDisSubm = iVal;
		}
		else if (IS_NAME("encImeOverwDisSubmNo"))
		{
			pConfig->meControl.encImeOverwDisSubmNo = iVal;
		}
		else if (IS_NAME("encIME2SearchRangeX"))
		{
			pConfig->meControl.encIME2SearchRangeX = iVal;
		}
		else if (IS_NAME("encIME2SearchRangeY"))
		{
			pConfig->meControl.encIME2SearchRangeY = iVal;
		}
		else
		{
			// Unknown
		}
    }
    else if (IS_SECTION("RDO"))
    {
		if (IS_NAME("encDisableTbePredIFrame"))
		{
			pConfig->rdoControl.encDisableTbePredIFrame = iVal;
		}
		else if (IS_NAME("encDisableTbePredPFrame"))
		{
			pConfig->rdoControl.encDisableTbePredPFrame = iVal;
		}
		else if (IS_NAME("useFmeInterpolY"))
		{
			pConfig->rdoControl.useFmeInterpolY = iVal;
		}
		else if (IS_NAME("useFmeInterpolUV"))
		{
			pConfig->rdoControl.useFmeInterpolUV = iVal;
		}
		else if (IS_NAME("enc16x16CostAdj"))
		{
			pConfig->rdoControl.enc16x16CostAdj = iVal;
		}
		else if (IS_NAME("encSkipCostAdj"))
		{
			pConfig->rdoControl.encSkipCostAdj = iVal;
		}
		else if (IS_NAME("encForce16x16skip"))
		{
			pConfig->rdoControl.encForce16x16skip = (unsigned char) iVal;
		}
		else
		{
			// Unknown
		}
    }
    else
    {
       /* unknown section */
    }
    return 1;

    #undef IS_SECTION
    #undef IS_NAME
}


/*******************************************************************************
 *  @fn     loadConfig
 *  @brief  Reads configuration file provided by user
 *  @param[in/out] pConfig        : Pointer to the configuration structure
 *  @param[in] configFilename : User configuration file name
 *  @return bool : true if successful; otherwise false.
 ******************************************************************************/
bool loadConfig (OvConfigCtrl *pConfig, char *configFilename)
{
	// DEFAULT CONFIG
    // fill-in the general configuration structures
    pConfig->encodeMode = (OVE_ENCODE_MODE) 1;
    pConfig->profileLevel.level						= 30;
    pConfig->profileLevel.profile					= 66;
    pConfig->pictFormat								= (OVE_PICTURE_FORMAT) 1;
    pConfig->priority								= (OVE_ENCODE_TASK_PRIORITY) 2;

    // fill-in the picture control structures
    pConfig->pictControl.size                       = sizeof(OVE_CONFIG_PICTURE_CONTROL);
    pConfig->pictControl.useConstrainedIntraPred	= 0;
    pConfig->pictControl.cabacEnable				= 0;
    pConfig->pictControl.cabacIDC					= 0;
    pConfig->pictControl.loopFilterDisable			= 0;
    pConfig->pictControl.encLFBetaOffset			= 0;
    pConfig->pictControl.encLFAlphaC0Offset			= 0;
    pConfig->pictControl.encIDRPeriod				= 0;
    pConfig->pictControl.encIPicPeriod				= 0;
    pConfig->pictControl.encHeaderInsertionSpacing  = 0;
    pConfig->pictControl.encCropLeftOffset			= 0;
    pConfig->pictControl.encCropRightOffset			= 0;
    pConfig->pictControl.encCropTopOffset			= 0;
    pConfig->pictControl.encCropBottomOffset		= 0;
    pConfig->pictControl.encNumMBsPerSlice		    = 99;
    pConfig->pictControl.encNumSlicesPerFrame		= 1;
    pConfig->pictControl.encForceIntraRefresh		= 0;
    pConfig->pictControl.encForceIMBPeriod			= 0;
    pConfig->pictControl.encInsertVUIParam			= 0;
    pConfig->pictControl.encInsertSEIMsg            = 0;

    // fill-in the rate control structures
    pConfig->rateControl.size                       = sizeof(OVE_CONFIG_RATE_CONTROL);
    pConfig->rateControl.encRateControlMethod		= 3;
    pConfig->rateControl.encRateControlTargetBitRate = 768000;
    pConfig->rateControl.encRateControlPeakBitRate  = 0;
    pConfig->rateControl.encRateControlFrameRateNumerator = 30;
    pConfig->rateControl.encRateControlFrameRateDenominator = 1;
    pConfig->rateControl.encGOPSize					= 0;
    pConfig->rateControl.encRCOptions				= 0;
    pConfig->rateControl.encVBVBufferSize			= 768000;
    pConfig->rateControl.encQP_I					= 22;
    pConfig->rateControl.encQP_P					= 22;
    pConfig->rateControl.encQP_B					= 0;

    // fill-in the motion estimation control structures
    pConfig->meControl.size                         = sizeof(OVE_CONFIG_MOTION_ESTIMATION);
    pConfig->meControl.imeDecimationSearch			= 1;
    pConfig->meControl.motionEstHalfPixel		    = 1;
    pConfig->meControl.motionEstQuarterPixel		= 1;
    pConfig->meControl.disableFavorPMVPoint			= 0;
    pConfig->meControl.forceZeroPointCenter			= 1;
    pConfig->meControl.lsmVert						= 0;
    pConfig->meControl.encSearchRangeX				= 16;
    pConfig->meControl.encSearchRangeY				= 16;
    pConfig->meControl.encSearch1RangeX				= 0;
    pConfig->meControl.encSearch1RangeY				= 0;
    pConfig->meControl.disable16x16Frame1			= 0;
    pConfig->meControl.disableSATD					= 0;
    pConfig->meControl.enableAMD					= 0;
    pConfig->meControl.encDisableSubMode			= 0;
    pConfig->meControl.encIMESkipX					= 0;
    pConfig->meControl.encIMESkipY					= 0;
    pConfig->meControl.encEnImeOverwDisSubm			= 0;
    pConfig->meControl.encImeOverwDisSubmNo			= 0;
    pConfig->meControl.encIME2SearchRangeX			= 4;
    pConfig->meControl.encIME2SearchRangeY			= 4;

    // fill-in the RDO control structures
    pConfig->rdoControl.size                        = sizeof(OVE_CONFIG_RDO);
    pConfig->rdoControl.encDisableTbePredIFrame		= 0;
    pConfig->rdoControl.encDisableTbePredPFrame		= 0;
    pConfig->rdoControl.useFmeInterpolY				= 0;
    pConfig->rdoControl.useFmeInterpolUV			= 0;
    pConfig->rdoControl.enc16x16CostAdj				= 0;
    pConfig->rdoControl.encSkipCostAdj				= 0;
    pConfig->rdoControl.encForce16x16skip			= (unsigned char) 0;

    // SET USER CONFIG
    if (ini_parse(configFilename, handler, pConfig) < 0)
    {
        printf("Can't load config file\n");
        return false;
    }

    return true;
}
