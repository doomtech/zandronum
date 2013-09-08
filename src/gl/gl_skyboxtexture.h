

//-----------------------------------------------------------------------------
//
// This is not a real texture but will be added to the texture manager
// so that it can be handled like any other sky.
//
//-----------------------------------------------------------------------------

class FSkyBox : public FTexture
{
public:

	FTexture * faces[6];
	bool fliptop;

	FSkyBox() 
	{ 
		faces[0]=faces[1]=faces[2]=faces[3]=faces[4]=faces[5]=NULL; 
		UseType=TEX_Override;
		gl_info.bSkybox = true;
		fliptop = false;
	}
	~FSkyBox()
	{
	}

	// If something attempts to use this as a texture just pass the information of the first face.
	virtual const BYTE *GetColumn (unsigned int column, const Span **spans_out)
	{
		if (faces[0]) return faces[0]->GetColumn(column, spans_out);
		return NULL;
	}
	virtual const BYTE *GetPixels ()
	{
		if (faces[0]) return faces[0]->GetPixels();
		return NULL;
	}
	virtual int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
	{
		if (faces[0]) return faces[0]->CopyTrueColorPixels(bmp, x, y, rotate, inf);
		return 0;
	}
	bool UseBasePalette() { return false; }	// not really but here it's not important.

	void SetSize()
	{
		if (faces[0]) 
		{
			Width=faces[0]->GetWidth();
			Height=faces[0]->GetHeight();
			CalcBitSize();
		}
	}

	virtual void Unload () 
	{
		for(int i=0;i<6;i++) if (faces[i]) faces[i]->Unload();
	}

	void PrecacheGL()
	{
		for(int i=0;i<6;i++) if (faces[i]) faces[i]->PrecacheGL();
	}
};
