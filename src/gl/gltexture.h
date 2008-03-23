
#ifndef __GLTEXTURE_H
#define __GLTEXTURE_H

#define SHADED_TEXTURE -1
#define DIRECT_PALETTE -2

#include "gl/gl_values.h"
#include "tarray.h"

class FCanvasTexture;
class AActor;

enum
{
	GLT_CLAMPX=1,
	GLT_CLAMPY=2
};

class GLTexture
{
	friend void gl_RenderTextureView(FCanvasTexture *Texture, AActor * Viewpoint, int FOV);

	enum
	{
		MAX_TEXTURES = 16
	};

	static struct TexFilter_s
	{
		int minfilter;
		int magfilter;
		bool mipmapping;
	} 
	TexFilter[];

	static struct TexFormat_s
	{
		int texformat;
	}
	TexFormat[];

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
	BYTE cm_arraysize;
	BYTE clampmode;

	unsigned int * glTexID;
	TArray<TranslatedTexture> glTexID_Translated;

	void LoadImage(unsigned char * buffer,int w, int h, unsigned int & glTexID,int wrapparam, bool alphatexture, int texunit);
	unsigned * GetTexID(int cm, int translation);

public:
	GLTexture(int w, int h, bool mip, bool wrap);
	~GLTexture();

	unsigned int Bind(int texunit, int cm, int translation=0, int clampmode = -1);
	unsigned int CreateTexture(unsigned char * buffer, int w, int h,bool wrap, int texunit, int cm, int translation=0);
	void Resize(int _width, int _height) ;
	void SetTextureClamp(int clampmode);

	void Clean(bool all);


	// Get right/bottom UV coordinates for patch drawing
	float GetUL() { return 0; }
	float GetVT() { return 0; }
	float GetUR() { return scalexfac; }
	float GetVB() { return scaleyfac; }
	float GetU(float upix) { return upix/(float)texwidth * scalexfac; }
	float GetV(float vpix) { return vpix/(float)texheight* scaleyfac; }

	// gets a texture coordinate from a pixel coordinate
	float FloatToTexU(float v) { return v/(float)texwidth; }
	float FixToTexU(int v) { return (float)v/(float)FRACUNIT/(float)texwidth; }
	float FixToTexV(int v) { return (float)v/(float)FRACUNIT/(float)texheight; }

};


#endif
