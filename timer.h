/*******************************************************************************
* This file is part of AvsVCEh264.
* Contains simple high-resolution Timer class
*
* Copyright (C) 2013 David González García <davidgg666@gmail.com>
*******************************************************************************/
#ifndef TIMER_H
#define TIMER_H

#include <stdlib.h>
#include <windows.h>

class Timer
{
  public:
    Timer()
    {
    	LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		mul = 1000000.0 / frequency.QuadPart;
    }

    ~Timer() {}

    inline void start()
    {
		endCount.QuadPart = 0;
		QueryPerformanceCounter(&startCount);
    }

    inline void stop()
    {
		QueryPerformanceCounter(&endCount);
    }

    double getElapsedTime()
	{
		return this->getInSec();
	}

    double getInSec()
    {
		return this->getInMicroSec() * 0.000001;
	}

    double getInMilliSec()
	{
		return this->getInMicroSec() * 0.001;
	}

    double getInMicroSec()
	{
		LARGE_INTEGER end;

		if(endCount.QuadPart == 0)
			QueryPerformanceCounter(&end);
		else
			end = endCount;

		return end.QuadPart * mul - startCount.QuadPart * mul;
	}

  private:
    double mul;
    LARGE_INTEGER startCount;
    LARGE_INTEGER endCount;
};

#endif
