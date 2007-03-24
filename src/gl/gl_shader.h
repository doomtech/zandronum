
#ifndef __GL_SHADERS_H__
#define __GL_SHADERS_H__


class FShader
{
	GLhandleARB hShader;
	GLhandleARB hVertProg;
	GLhandleARB hFragProg;

	TArray<int> params;
	TArray<int> param_names;

public:
	FShader()
	{
		hShader = hVertProg = hFragProg = NULL;
	}

	~FShader();

	bool Load(const char * name, const char * vp_path, const char * fp_path);
	void AddParameter(const char * pname);
	int GetParameterIndex(FName pname);
	bool Bind();

};


/*

class FShaderManager
{
public:

	FShaderManager();
	~FShaderManager();

	TArray<FShader *> m_Shaders;
	void AddShader(FShader * shader);
	bool BindShader(name shadername);

}
*/


#endif