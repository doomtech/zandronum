#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

#endif

#define USE_WINDOWS_DWORD
#include "i_system.h"
#include "gl/common/glc_clock.h"


glcycle_t RenderWall,SetupWall,ClipWall;
glcycle_t RenderFlat,SetupFlat;
glcycle_t RenderSprite,SetupSprite;
glcycle_t All, Finish, PortalAll;
glcycle_t ProcessAll;
glcycle_t RenderAll;
glcycle_t Dirty;
int vertexcount, flatvertices, flatprimitives;

int rendered_lines,rendered_flats,rendered_sprites,render_vertexsplit,render_texsplit,rendered_decals;
int iter_dlightf, iter_dlight, draw_dlight, draw_dlightf;

double		gl_SecondsPerCycle = 1e-8;
double		gl_MillisecPerCycle = 1e-5;		// 100 MHz

// For GL timing the performance counter is far too costly so we still need RDTSC
// even though it may not be perfect.

void gl_CalculateCPUSpeed ()
{
	#ifdef WIN32
		LARGE_INTEGER freq;

		QueryPerformanceFrequency (&freq);

		if (freq.QuadPart != 0)
		{
			LARGE_INTEGER count1, count2;
			unsigned minDiff;
			long long ClockCalibration = 0;

			// Count cycles for at least 55 milliseconds.
			// The performance counter is very low resolution compared to CPU
			// speeds today, so the longer we count, the more accurate our estimate.
			// On the other hand, we don't want to count too long, because we don't
			// want the user to notice us spend time here, since most users will
			// probably never use the performance statistics.
			minDiff = freq.LowPart * 11 / 200;

			// Minimize the chance of task switching during the testing by going very
			// high priority. This is another reason to avoid timing for too long.
			SetPriorityClass (GetCurrentProcess (), REALTIME_PRIORITY_CLASS);
			SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_TIME_CRITICAL);
			ClockCalibration = __rdtsc();
			QueryPerformanceCounter (&count1);
			do
			{
				QueryPerformanceCounter (&count2);
			} while ((DWORD)((unsigned __int64)count2.QuadPart - (unsigned __int64)count1.QuadPart) < minDiff);
			ClockCalibration = __rdtsc() - ClockCalibration;
			QueryPerformanceCounter (&count2);
			SetPriorityClass (GetCurrentProcess (), NORMAL_PRIORITY_CLASS);
			SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_NORMAL);

			double CyclesPerSecond = (double)ClockCalibration *
				(double)freq.QuadPart /
				(double)((__int64)count2.QuadPart - (__int64)count1.QuadPart);
			gl_SecondsPerCycle = 1.0 / CyclesPerSecond;
			gl_MillisecPerCycle = 1000.0 / CyclesPerSecond;
		}
	#endif
}


//-----------------------------------------------------------------------------
//
// Rendering statistics
//
//-----------------------------------------------------------------------------
ADD_STAT(rendertimes)
{
	static FString buff;
	static int lasttime=0;
	int t=I_MSTime();
	if (t-lasttime>1000) 
	{
		buff.Format("W: Render=%2.2f, Setup=%2.2f, Clip=%2.2f\nF: Render=%2.2f, Setup=%2.2f\nS: Render=%2.2f, Setup=%2.2f\nAll: All=%2.2f, Render=%2.2f, Setup=%2.2f, Portal=%2.2f, Finish=%2.2f\n",
		RenderWall.TimeMS(), SetupWall.TimeMS(), ClipWall.TimeMS(), RenderFlat.TimeMS(), SetupFlat.TimeMS(),
		RenderSprite.TimeMS(), SetupSprite.TimeMS(), All.TimeMS() + Finish.TimeMS(), RenderAll.TimeMS(),
		ProcessAll.TimeMS(), PortalAll.TimeMS(), Finish.TimeMS());
		lasttime=t;
	}
	return buff;
}

ADD_STAT(renderstats)
{
	FString out;
	out.Format("Walls: %d (%d splits, %d t-splits, %d vertices)\n, Flats: %d (%d primitives, %d vertices)\n, Sprites: %d, Decals=%d\n", 
		rendered_lines, render_vertexsplit, render_texsplit, vertexcount, rendered_flats, flatprimitives, flatvertices, rendered_sprites,rendered_decals );
	return out;
}

ADD_STAT(lightstats)
{
	FString out;
	out.Format("DLight - Walls: %d processed, %d rendered - Flats: %d processed, %d rendered\n", 
		iter_dlight, draw_dlight, iter_dlightf, draw_dlightf );
	return out;
}


