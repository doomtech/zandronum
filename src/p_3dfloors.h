#ifndef __SECTORE_H
#define __SECTORE_H

#define ML_3DMIDTEX		0x40000000


// 3D floor flags. Most are the same as in Legacy but I added some for EDGE's and Vavoom's features as well.

typedef enum
{
  FF_EXISTS            = 0x1,    //MAKE SURE IT'S VALID
  FF_SOLID             = 0x2,    //Does it clip things?
  FF_RENDERSIDES       = 0x4,    //Render the sides?
  FF_RENDERPLANES      = 0x8,    //Render the floor/ceiling?
  FF_RENDERALL         = 0xC,    //Render everything?
  FF_SWIMMABLE         = 0x10,   //Can we swim?
  FF_NOSHADE           = 0x20,   //Does it mess with the lighting?
  FF_BOTHPLANES        = 0x200,  //Render both planes all the time?
  FF_TRANSLUCENT       = 0x800,  //See through!
  FF_FOG               = 0x1000, //Fog "brush"?
  FF_INVERTPLANES      = 0x2000, //Reverse the plane visibility rules?
  FF_ALLSIDES          = 0x4000, //Render inside and outside sides?
  FF_INVERTSIDES       = 0x8000, //Only render inside sides?
  FF_DOUBLESHADOW      = 0x10000,//Make two lightlist entries to reset light?
  FF_UPPERTEXTURE	   = 0x20000,
  FF_LOWERTEXTURE      = 0x40000,
  FF_THINFLOOR		   = 0x80000,	// EDGE
  FF_SCROLLY           = 0x100000,  // EDGE - not yet implemented!!!
  FF_FIX			   = 0x200000,  // use floor of model sector as floor and floor of real sector as ceiling
  FF_INVERTSECTOR	   = 0x400000,	// swap meaning of sector planes
  FF_DYNAMIC		   = 0x800000,	// created by partitioning another 3D-floor due to overlap
  FF_CLIPPED		   = 0x1000000,	// split into several dynamic ffloors
  FF_SEETHROUGH        = 0x2000000,
  FF_SHOOTTHROUGH      = 0x4000000,
  FF_FADEWALLS         = 0x8000000,	// Applies real fog to walls and doesn't blend the view		
  FF_ADDITIVETRANS	   = 0x10000000, // Render this floor with additive translucency
  
} ffloortype_e;


struct secplane_t;

struct F3DFloor
{
	struct planeref
	{
		secplane_t *	plane;
		int *			texture;
		fixed_t *		texheight;
		sector_t *		model;
		bool			isceiling;
	};

	planeref			bottom;
	planeref			top;

	unsigned char		*toplightlevel;
	
	fixed_t				delta;
	
	int					flags;
	line_s*				master;
	
	sector_t *			model;
	sector_t *			target;
	
	int					lastlight;
	int					alpha;
	
};


struct FDynamicColormap;


typedef struct lightlist_s {
	secplane_t				plane;
	unsigned char *			p_lightlevel;
	FDynamicColormap **		p_extra_colormap;
	int						flags;
	F3DFloor*				caster;
} lightlist_t;


// this substructure contains a few sector properties that are stored in dynamic arrays
// These must not be copied by R_FakeFlat etc. or bad things will happen!
struct extsector_t
{
	// 3D floor stuff
	TArray<F3DFloor *>		ffloors;		// 3D floors in this sector
	TArray<lightlist_t>		lightlist;		// 3D light list
	TArray<sector_t*>		attached;		// 3D floors attached to this sector
};

class player_s;
void P_PlayerOnSpecial3DFloor(player_s* player);
void P_LineOpening (AActor * thing,const line_s *linedef, fixed_t x, fixed_t y, fixed_t refx=FIXED_MIN, fixed_t refy=0);

void P_Get3DFloorAndCeiling(AActor * thing, sector_t * sector, fixed_t * floorz, fixed_t * ceilingz, int * floorpic);
bool P_CheckFor3DFloorHit(AActor * mo);
bool P_CheckFor3DCeilingHit(AActor * mo);
void P_Recalculate3DFloors(sector_t *);
void P_RecalculateAttached3DFloors(sector_t * sec);
lightlist_t * P_GetPlaneLight(sector_t * , secplane_t * plane, bool underside);
bool P_GetMidTexturePosition(const line_s * linedef, int side, fixed_t * ptextop, fixed_t * ptexbot);
void P_SpawnSpecials2( void );
void P_CreateExtSectors();
void P_CleanExtSectors();

#endif

