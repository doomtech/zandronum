#ifndef GLSL_STATE_H
#define GLSL_STATE_H

#include "doomtype.h"

struct FColormap;

class GLSLRenderState
{


public:

	void SetFixedColormap(int cm);
	void SetLight(int light, int rellight, FColormap *cm, float alpha, bool additive);
	void SetLight(int light, int rellight, FColormap *cm, float alpha, bool additive, PalEntry ThingColor);
	void SetAddLight(float *f);
	void SetLightAbsolute(float r, float g, float b, float a);
	void EnableFog(bool on);
	void SetWarp(int mode, float speed);
	void EnableTexture(bool on);
	void EnableBrightmap(bool on);
	void EnableFogBoundary(bool on);
	void SetBrightmap(bool on);
	void SetFloorGlow(PalEntry color, float height);
	void SetCeilingGlow(PalEntry color, float height);
	void ClearDynLights();
	void SetDynLight(int index, float x, float y, float z, float size, PalEntry color, int mode);
	void SetLightingMode(int mode);
	void SetAlphaThreshold(float thresh);
	void SetBlend(int src, int dst);
	void SetTextureMode(int mode);
	void Apply();
	void Enable(bool on);
	void Reset();
};

extern GLSLRenderState *glsl;


#endif