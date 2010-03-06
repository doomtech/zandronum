#ifndef __GL_MODELS_H_
#define __GL_MODELS_H_

#include "r_data.h"
#include "gl_geometric.h"

#define MAX_LODS			4

enum { VX, VZ, VY };

static const float rModelAspectMod = 1 / 1.2f;	//.833334f;

#define MD2_MAGIC			0x32504449
#define DMD_MAGIC			0x4D444D44
#define MD3_MAGIC			0x33504449
#define NUMVERTEXNORMALS	162

FTexture * LoadSkin(const char * path, const char * fn);


class FModel
{
public:
	FModel() { filename = NULL; }
	virtual ~FModel() { if (filename!=NULL) delete [] filename; }

	virtual bool Load(const char * fn, const char * buffer, int length) = 0;
	virtual int FindFrame(const char * name) = 0;
	virtual void RenderFrame(FTexture * skin, int frame, int cm, Matrix3x4 *m2v, int translation=0) = 0;
	// [BB] Added RenderFrameInterpolated
	virtual void RenderFrameInterpolated(FTexture * skin, int frame, int frame2, double inter, int cm, Matrix3x4 *m2v, int translation=0) = 0;

	char * filename;
};

class FDMDModel : public FModel
{
protected:

	struct FTriangle
	{
		short           vertexIndices[3];
		short           textureIndices[3];
	};


	struct DMDHeader
	{
		int             magic;
		int             version;
		int             flags;
	};

	struct FModelVertex
	{
		float           xyz[3];
	};

	struct FTexCoord
	{
		short           s, t;
	};

	struct FGLCommandVertex
	{
		float           s, t;
		int             index;
	};

	struct DMDInfo
	{
		int             skinWidth;
		int             skinHeight;
		int             frameSize;
		int             numSkins;
		int             numVertices;
		int             numTexCoords;
		int             numFrames;
		int             numLODs;
		int             offsetSkins;
		int             offsetTexCoords;
		int             offsetFrames;
		int             offsetLODs;
		int             offsetEnd;
	};

	struct ModelFrame
	{
		char            name[16];
		FModelVertex *vertices;
		FModelVertex *normals;

		ModelFrame()
		{
			vertices = normals = NULL;
		}

		~ModelFrame()
		{
			if (vertices) delete [] vertices;
			if (normals) delete [] normals;
		}
	};

	struct DMDLoDInfo
	{
		int             numTriangles;
		int             numGlCommands;
		int             offsetTriangles;
		int             offsetGlCommands;
	};

	struct DMDLoD
	{
		FTriangle		* triangles;
		int				* glCommands;
	};


	bool			loaded;
	DMDHeader	    header;
	DMDInfo			info;
	FTexture **		skins;
	FTexCoord *		texCoords;
	
	ModelFrame  *	frames;
	DMDLoDInfo		lodInfo[MAX_LODS];
	DMDLoD			lods[MAX_LODS];
	char           *vertexUsage;   // Bitfield for each vertex.
	bool			allowTexComp;  // Allow texture compression with this.

	static void RenderGLCommands(void *glCommands, unsigned int numVertices,FModelVertex * vertices, Matrix3x4 *modeltoworld);

public:
	FDMDModel() { loaded = false; }
	virtual ~FDMDModel();

	virtual bool Load(const char * fn, const char * buffer, int length);
	virtual int FindFrame(const char * name);
	virtual void RenderFrame(FTexture * skin, int frame, int cm, Matrix3x4 *m2v, int translation=0);
	virtual void RenderFrameInterpolated(FTexture * skin, int frame, int frame2, double inter, int cm, Matrix3x4 *m2v, int translation=0);

};

// This uses the same internal representation as DMD
class FMD2Model : public FDMDModel
{
public:
	FMD2Model() {}
	virtual ~FMD2Model();

	virtual bool Load(const char * fn, const char * buffer, int length);

};


class FMD3Model : public FModel
{
	struct MD3Tag
	{
		// Currently I have no use for this
	};

	struct MD3TexCoord
	{
		float s,t;
	};

	struct MD3Vertex
	{
		float x,y,z;
		float nx,ny,nz;
	};

	struct MD3Triangle
	{
		int VertIndex[3];
	};

	struct MD3Surface
	{
		int numVertices;
		int numTriangles;
		int numSkins;

		FTexture ** skins;
		MD3Triangle * tris;
		MD3TexCoord * texcoords;
		MD3Vertex * vertices;

		MD3Surface()
		{
			tris=NULL;
			vertices=NULL;
			texcoords=NULL;
		}

		~MD3Surface()
		{
			if (tris) delete [] tris;
			if (vertices) delete [] vertices;
			if (texcoords) delete [] texcoords;
			for (int i=0;i<numSkins;i++) delete skins[i];
			if (skins) delete [] skins;
		}
	};

	struct MD3Frame
	{
		// The bounding box information is of no use in the Doom engine
		// That will still be done with the actor's size information.
		char Name[16];
		float origin[3];
	};

	int numFrames;
	int numTags;
	int numSurfaces;

	MD3Frame * frames;
	MD3Surface * surfaces;

	void RenderTriangles(MD3Surface * surf, MD3Vertex * vert, Matrix3x4 *modeltoworld);

public:
	FMD3Model() { }
	virtual ~FMD3Model();

	virtual bool Load(const char * fn, const char * buffer, int length);
	virtual int FindFrame(const char * name);
	virtual void RenderFrame(FTexture * skin, int frame, int cm, Matrix3x4 *m2v, int translation=0);
	virtual void RenderFrameInterpolated(FTexture * skin, int frame, int frame2, double inter, int cm, Matrix3x4 *m2v, int translation=0);
};



#define MAX_MODELS_PER_FRAME 4

//
// [BB] Model rendering flags.
//
typedef enum
{
	// [BB] Color translations for the model skin are ignored. This is
	// useful if the skin texture is not using the game palette.
	MDL_IGNORETRANSLATION			= 1,
	MDL_PITCHFROMMOMENTUM			= 2,
	MDL_ROTATING					= 4,
	MDL_INTERPOLATEDOUBLEDFRAMES	= 8,
	MDL_NOINTERPOLATION				= 16,
	MDL_ALIGNANGLE					= 32,
	MDL_ALIGNPITCH					= 64,
	MDL_ROLLAGAINSTANGLE			= 128,
};

struct FSpriteModelFrame
{
	FModel * models[MAX_MODELS_PER_FRAME];
	FTexture * skins[MAX_MODELS_PER_FRAME];
	int modelframes[MAX_MODELS_PER_FRAME];
	float xscale, yscale, zscale;
	// [BB] Added zoffset, rotation parameters and flags.
	float zoffset;
	float xrotate, yrotate, zrotate;
	float rotationCenterX, rotationCenterY, rotationCenterZ;
	float rotationSpeed;
	unsigned int flags;
	const PClass * type;
	short sprite;
	short frame;
	FState * state;	// for later!
	int hashnext;
};

class GLSprite;

void gl_InitModels();
FSpriteModelFrame * gl_FindModelFrame(const PClass * ti, int sprite, int frame);
void gl_RenderModel(GLSprite * spr, int cm);
// [BB] HUD weapon model rendering functions.
void gl_RenderHUDModel(pspdef_t *psp, fixed_t ofsx, fixed_t ofsy, int cm);
bool gl_IsHUDModelForPlayerAvailable (player_t * player);

#endif
