
#ifndef __GLTEXTURE_H
#define __GLTEXTURE_H

#define SHADED_TEXTURE -1
#define DIRECT_PALETTE -2

#include "tarray.h"

class FCanvasTexture;
class AActor;

void gl_RenderTextureView(FCanvasTexture *Texture, AActor * Viewpoint, int FOV);

enum
{
	GLT_CLAMPX=1,
	GLT_CLAMPY=2
};

class FHardwareTexture
{
	friend void gl_RenderTextureView(FCanvasTexture *Texture, AActor * Viewpoint, int FOV);

	enum
	{
		MAX_TEXTURES = 16
	};

	struct TranslatedTexture
	{
		unsigned int glTexID;
		int translation;
		int cm;
	};

public:

	static unsigned int lastbound[MAX_TEXTURES];
	static int lastactivetexture;
	static bool supportsNonPower2;
	static int max_texturesize;

	static int GetTexDimension(int value);

private:

	short texwidth, texheight;
	float scalexfac, scaleyfac;
	bool mipmap;
	BYTE clampmode;

	unsigned int * glTexID;
	TArray<TranslatedTexture> glTexID_Translated;
	unsigned int glDepthID;	// only used by camera textures

	void LoadImage(unsigned char * buffer,int w, int h, unsigned int & glTexID,int wrapparam, bool alphatexture, int texunit);
	unsigned * GetTexID(int cm, int translation);

	int GetDepthBuffer();
	void DeleteTexture(unsigned int texid);

public:
	FHardwareTexture(int w, int h, bool mip, bool wrap);
	~FHardwareTexture();

	static void Unbind(int texunit);
	static void UnbindAll();

	void BindToFrameBuffer();

	unsigned int Bind(int texunit, int cm, int translation=0);
	unsigned int CreateTexture(unsigned char * buffer, int w, int h,bool wrap, int texunit, int cm, int translation=0);
	void Resize(int _width, int _height) ;

	void Clean(bool all);


	// Get right/bottom UV coordinates for patch drawing
	float GetUL() const { return 0; }
	float GetVT() const { return 0; }
	float GetUR() const { return scalexfac; }
	float GetVB() const { return scaleyfac; }
	float GetU(float upix) const { return upix/(float)texwidth * scalexfac; }
	float GetV(float vpix) const { return vpix/(float)texheight* scaleyfac; }
	float GetWidth () const { return (float)texwidth * scalexfac; }
	float GetHeight() const { return (float)texheight* scaleyfac; }

	// gets a texture coordinate from a pixel coordinate
	float FloatToTexU(float v) const { return v/(float)texwidth; }
	float FloatToTexV(float v) const { return v/(float)texheight; }

};

#endif
