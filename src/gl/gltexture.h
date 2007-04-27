
#ifndef __GLTEXTURE_H
#define __GLTEXTURE_H

#define SHADED_TEXTURE -1
#define DIRECT_PALETTE -2

#include "gl/gl_values.h"
#include "tarray.h"

class FCanvasTexture;
class AActor;

class GLTexture
{
	friend void gl_RenderTextureView(FCanvasTexture *Texture, AActor * Viewpoint, int FOV);

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

	static unsigned int lastbound;
	static bool supportsNonPower2;
	static int max_texturesize;

	static int GetTexDimension(int value);

private:

	short texwidth, texheight;
	float scalexfac, scaleyfac;
	bool mipmap;
	BYTE cm_arraysize;

	unsigned int * glTexID;
	TArray<TranslatedTexture> glTexID_Translated;

	void LoadImage(unsigned char * buffer,int w, int h, unsigned int & glTexID,int wrapparam, bool alphatexture=false);
	unsigned * GetTexID(int cm, int translation, const unsigned char * translationtbl);

public:
	GLTexture(int w, int h, bool mip, bool wrap);
	~GLTexture();

	unsigned int Bind(int cm, int translation=0, const unsigned char * translationtbl=NULL);
	unsigned int CreateTexture(unsigned char * buffer, int w, int h,bool wrap, int cm, int translation=0, const unsigned char * translationtbl=NULL);
	void Resize(int _width, int _height) ;

	void Clean(bool all);


	// Get right/bottom UV coordinates for patch drawing
	float GetUR() { return scalexfac; } // (float)realtexwidth/(float)tex_width; }
	float GetVB() { return scaleyfac; } //(float)realtexheight/(float)tex_height; }
	float GetU(float upix) { return upix/(float)texwidth * scalexfac; }
	float GetV(float vpix) { return vpix/(float)texheight* scaleyfac; }

	// gets a texture coordinate from a pixel coordinate
	float FloatToTexU(float v) { return v/(float)texwidth; }
	float FixToTexU(int v) { return (float)v/(float)FRACUNIT/(float)texwidth; }
	float FixToTexV(int v) { return (float)v/(float)FRACUNIT/(float)texheight; }
};


#endif
