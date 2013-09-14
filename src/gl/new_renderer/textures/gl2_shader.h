#ifndef __GL_SHADER_H
#define __GL_SHADER_H


#include "name.h"
#include "tarray.h"

namespace GLRendererNew
{

//----------------------------------------------------------------------------
//
// Shader properties with caching mechanism
//
//----------------------------------------------------------------------------

class FShaderProperty
{
protected:
	int index;

public:
	FShaderProperty()
	{
		index = -1;
	}

	void setIndex(GLhandleARB handle, const char *name)
	{
		index = gl.GetUniformLocation(handle, name);
	}
};


class FShaderPropertyInt : public FShaderProperty
{
	int cached_value;

public:
	FShaderPropertyInt()
	{
		cached_value = INT_MAX;
	}

	void Set(int value)
	{
		if (value != cached_value)
		{
			cached_value = value;
			gl.Uniform1i(index, value);
		}
	}
};

class FShaderPropertyFloat : public FShaderProperty
{
	float cached_value;

public:
	FShaderPropertyFloat()
	{
		cached_value = FLT_MAX;
	}

	void Set(float value)
	{
		if (value != cached_value)
		{
			cached_value = value;
			gl.Uniform1f(index, value);
		}
	}
};

class FShaderPropertyVector : public FShaderProperty
{
	float cached_value[4];

public:
	FShaderPropertyVector()
	{
		cached_value[0] = FLT_MAX;
	}

	void Set(float *value)
	{
		if (memcmp(value, cached_value, sizeof(cached_value)))
		{
			memcpy(cached_value, value, sizeof(cached_value));
			gl.Uniform4fv(index, 1, value);
		}
	}
};

class FShaderPropertyMatrix4x4 : public FShaderProperty
{
	float cached_value[16];

public:
	FShaderPropertyMatrix4x4()
	{
		cached_value[0] = FLT_MAX;
	}

	void Set(float *value)
	{
		if (memcmp(value, cached_value, sizeof(cached_value)))
		{
			memcpy(cached_value, value, sizeof(cached_value));
			gl.UniformMatrix4fv(index, 1, false, value);
		}
	}
};

//----------------------------------------------------------------------------
//
// Shader system object
//
//----------------------------------------------------------------------------

class FShaderObject
{
	friend class FShader;

	GLhandleARB hShader;
	GLhandleARB hVertProg;
	GLhandleARB hFragProg;

	FShaderPropertyInt mTimer;
	FShaderPropertyFloat mDesaturationFactor;
	FShaderPropertyInt mFogRadial;
	FShaderPropertyInt mTextureMode;
	FShaderPropertyVector mCameraPos;
	FShaderPropertyVector mColormapColor;

	enum AttributeIndices
	{
		attrLightParams = 11,
		attrFogColor,
		attrGlowDistance,
		attrGlowTopColor,
		attrGlowBottomColor,
	};

public:

	FShaderObject();
	~FShaderObject();
	bool Create(const char *name, const char *vertexshader, const char *fragmentshader);
	void Bind();

	void setTimer(int time)
	{
		mTimer.Set(time);
	}

	void setDesaturationFactor(float fac)
	{
		mDesaturationFactor.Set(fac);
	}

	void setFogType(int type)
	{
		mFogRadial.Set(type);
	}

	void setTextureMode(int mode)
	{
		mTextureMode.Set(mode);
	}

	void setCameraPos(float *vec)
	{
		mCameraPos.Set(vec);
	}

	void setColormapColor(float *vec)
	{
		mColormapColor.Set(vec);
	}
};

//----------------------------------------------------------------------------
//
// Shader application object
//
//----------------------------------------------------------------------------
class FShaderContainer;

class FShader
{
	friend class FShaderContainer;

	FName mName;
	FShaderObject *mBaseShader;
	FShaderObject *mColormapShader;
	FShaderContainer *mOwner;

	FShaderObject *CreateShader(const char *vp, const char *fp,const char * filename_pixfunc);

public:

	FShader(const char *shadername);
	virtual ~FShader();

	bool Create(const char * filename_pixfunc);
	FName GetName() { return mName; }

	virtual void Bind(float *cm, int texturemode, float desaturation, float Speed);
	//static void Unbind();

};

//----------------------------------------------------------------------------
//
// Shader container object
//
//----------------------------------------------------------------------------

class FShaderContainer
{
	TArray<FShader *> mShaders;
	FShaderObject *mActiveShader;

	int mFogType;
	float mCameraPos[4];

public:

	FShaderContainer()
	{
		mActiveShader = NULL;
		mFogType = -1;
		mCameraPos[0] = 
		mCameraPos[1] = 
		mCameraPos[2] = 
		mCameraPos[3] = -1e30;
		CreateDefaultShaders();
	}
	
	~FShaderContainer();

	bool CreateDefaultShaders();
	void AddShader(FShader *shader);
	FShader *FindShader(const char *name);

	void SetFogType(int type)
	{
		mFogType = type;
		if (mActiveShader != NULL) mActiveShader->setFogType(type);
	}

	void SetCameraPos(float *vec)
	{
		memcpy(mCameraPos, vec, sizeof(mCameraPos));
		if (mActiveShader != NULL) mActiveShader->setCameraPos(vec);
	}

	void SetActiveShader(FShaderObject *active);

};


}
#endif