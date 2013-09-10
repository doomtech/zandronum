#ifndef __GL_RENDERHACKS_H
#define __GL_RENDERHACKS_H

//==========================================================================
//
// these are used to link faked planes due to missing textures to a sector
//
//==========================================================================
struct gl_subsectorrendernode
{
	gl_subsectorrendernode *	next;
	subsector_t *				sub;
};


struct FRenderHackInfo
{

	struct MissingTextureInfo
	{
		seg_t * seg;
		subsector_t * sub;
		fixed_t planez;
		fixed_t planezfront;
	};

	struct MissingSegInfo
	{
		seg_t * seg;
		int MTI_Index;	// tells us which MissingTextureInfo represents this seg.
	};

	struct SubsectorHackInfo
	{
		subsector_t * sub;
		BYTE flags;
	};

	TArray<BYTE> sectorrenderflags;
	TArray<BYTE> ss_renderflags;

	TArray<MissingTextureInfo> MissingUpperTextures;
	TArray<MissingTextureInfo> MissingLowerTextures;

	TArray<MissingSegInfo> MissingUpperSegs;
	TArray<MissingSegInfo> MissingLowerSegs;

	TArray<SubsectorHackInfo> SubsectorHacks;

	TArray<gl_subsectorrendernode*> otherfloorplanes;
	TArray<gl_subsectorrendernode*> otherceilingplanes;

	TArray<subsector_t *> CeilingStacks;
	TArray<subsector_t *> FloorStacks;

	TArray<subsector_t *> HandledSubsectors;

	void StartScene();

	bool DoOneSectorUpper(subsector_t * subsec, fixed_t planez);
	bool DoOneSectorLower(subsector_t * subsec, fixed_t planez);
	bool DoFakeBridge(subsector_t * subsec, fixed_t planez);
	bool DoFakeCeilingBridge(subsector_t * subsec, fixed_t planez);

	bool CheckAnchorFloor(subsector_t * sub);
	bool CollectSubsectorsFloor(subsector_t * sub, sector_t * anchor);
	bool CheckAnchorCeiling(subsector_t * sub);
	bool CollectSubsectorsCeiling(subsector_t * sub, sector_t * anchor);
	void CollectSectorStacksCeiling(subsector_t * sub, sector_t * anchor);
	void CollectSectorStacksFloor(subsector_t * sub, sector_t * anchor);

	void AddUpperMissingTexture(seg_t * seg, fixed_t backheight);
	void AddLowerMissingTexture(seg_t * seg, fixed_t backheight);
	void HandleMissingTextures();
	void DrawUnhandledMissingTextures();
	void AddHackedSubsector(subsector_t * sub);
	void HandleHackedSubsectors();
	void AddFloorStack(subsector_t * sub);
	void AddCeilingStack(subsector_t * sub);
	void ProcessSectorStacks();

	void AddOtherFloorPlane(int sector, gl_subsectorrendernode * node);
	void AddOtherCeilingPlane(int sector, gl_subsectorrendernode * node);

	gl_subsectorrendernode * GetOtherFloorPlanes(unsigned int sector)
	{
		if (sector<otherfloorplanes.Size()) return otherfloorplanes[sector];
		else return NULL;
	}

	gl_subsectorrendernode * GetOtherCeilingPlanes(unsigned int sector)
	{
		if (sector<otherceilingplanes.Size()) return otherceilingplanes[sector];
		else return NULL;
	}

	virtual void FloodUpperGap(seg_t * seg) = 0;
	virtual void FloodLowerGap(seg_t * seg) = 0;
};


#endif
