#ifndef __GL_RENDERSTATE_H
#define __GL_RENDERSTATE_H

#include <string.h>

class FRenderState
{
	bool mFogEnabled;
	bool mGlowEnabled;
	bool mLightEnabled;
	bool mBrightmapEnabled;


	bool ffFogEnabled;

public:
	FRenderState()
	{
		mBrightmapEnabled = mFogEnabled = mGlowEnabled = mLightEnabled = false;
		ffFogEnabled = false;
	}

	void Apply(bool forcenoshader = false);

	void EnableFog(bool on)
	{
		mFogEnabled = on;
	}

	void EnableGlow(bool on)
	{
		mGlowEnabled = on;
	}

	void EnableLight(bool on)
	{
		mLightEnabled = on;
	}

	void EnableBrightmap(bool on)
	{
		mBrightmapEnabled = on;
	}

	bool isFogEnabled()
	{
		return mFogEnabled;
	}

	bool isGlowEnabled()
	{
		return mGlowEnabled;
	}

	bool isLightEnabled()
	{
		return mLightEnabled;
	}

	bool isBrightmapEnabled()
	{
		return mBrightmapEnabled;
	}

};

extern FRenderState gl_RenderState;

#endif