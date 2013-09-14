#ifndef __GL_HWTEXTURE_H
#define __GL_HWTEXTURE_H

class FTexture;
namespace GLRendererNew
{

	class FHardwareTexture
	{
		unsigned int mTextureID;
		static unsigned LastBound[32];

	public:

		FHardwareTexture();
		~FHardwareTexture();
		bool Create(const unsigned char *, int w, int h, bool mipmap, int texformat);
		bool Bind(int texunit);
	};

}


#endif