#ifndef __GL2_SKYDRAW_H
#define __GL2_SKYDRAW_H

#include "tarray.h"
#include "tables.h"
#include "gl/new_renderer/gl2_vertex.h"

namespace GLRendererNew
{

	struct FVertexSky
	{
		float x,y,z;
		float u,v;
		unsigned char r,g,b,a;
	};

	struct FPrimitiveSky
	{
		int mType;
		int mStartVertex;
		int mVertexCount;
		int mUseTexture;
	};

	class FSkyDrawer;

	enum ESkyVBOType
	{
		SKYVBO_Dome,
		SKYVBO_Box6,
		SKYVBO_Box3,
	};

	class FVertexBufferSky : public FVertexBuffer
	{
		friend class FSkyDrawer;

		int type;
		bool stretch;
		FTexture* tex[2];
		PalEntry fogcolor;

		int lastused;
		TArray<FPrimitiveSky> mPrimitives;

	public:
		FVertexBufferSky();
		bool Bind();
		void SetVertices(FVertexSky *verts, unsigned int count);
	};

	class FSkyDrawer
	{
		static const unsigned maxSideAngle = unsigned(ANGLE_180) / 3;
		static const int rows = 4;
		static const int columns = 64;
		static const int scale = 10000 << FRACBITS;

		// cache the most recently used VBOs used for the sky in the current level.
		FVertexBufferSky *mSkyVBOs[5];

		void SkyVertex(FVertexSky *dest, int r, int c, int texw, float yMult, bool yflip, bool textured);
		void GenerateHemispheres(int texno, FTexture *tex, TArray<FPrimitiveSky> &prims, TArray<FVertexSky> &verts);
		FVertexBufferSky *FindVBO(int type, FTexture* t1, FTexture* t2, PalEntry fogcolor, bool stretch);
		FVertexBufferSky *CreateDomeVBO(FTexture *tex1, FTexture *tex2, PalEntry fogcolor);
		FVertexBufferSky *CreateBox6VBO();
		FVertexBufferSky *CreateBox3VBO();
		FVertexBufferSky *CreateFogLayerVBO();
		void CacheVBO(FVertexBufferSky *vbo);


	public:
		FSkyDrawer();
		~FSkyDrawer();
		void Clear();
		void RenderSky(FTextureID tex1, FTextureID tex2, PalEntry fogcolor, float xofs1, float xofs2, float yofs);
	};

}

#endif