/*******************************************************************************
* This file is part of AvsVCEh264.
* Contains functions for working with avisynth
*
* Copyright (C) 2013 David González García <davidgg666@gmail.com>
*******************************************************************************/
#include <stdio.h>
#include "avisynth_c.h"

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
        fprintf(stderr, "avs files return value is not a clip.\n");
        return 0;
    }

    clip = avs_take_clip(val_return, env);

    avs_release_value(val_return);

    return clip;
}
