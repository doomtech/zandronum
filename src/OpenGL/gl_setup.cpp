
#include "gl_main.h"
#include "w_wad.h"


typedef struct
{
	int x, y;
} glvert2_t;


typedef struct
{
   unsigned short startVert, endVert, lineDef, sideNum, partnerSeg;
} glseg_t;

int numGLverts;


// this loads the GL_VERT lump
void P_LoadGLVertexes(int lumpnum)
{
   FWadLump data = Wads.OpenLumpNum (lumpnum);
   DWORD id;
   int i;
   vertex_t *oldVerts;
   bool isV2;
   SWORD mx, my;
   DWORD vx, vy;

   data.Read(&id, 4);
   oldVerts = vertexes;
   if (id == MAKE_ID('g', 'N', 'd', '2'))
   {
      //v2 vertexes
      numGLverts = (data.GetLength() - 4) / sizeof(glvert2_t);
      isV2 = true;
   }
   else
   {
      //v1 vertexes
      // rewind file pointer since v1 didn't have the header
      data.Seek(0, SEEK_SET);
      numGLverts = data.GetLength() / sizeof(mapvertex_t);
      isV2 = false;
   }

   // resize vertex array
   vertexes = new vertex_t[numvertexes + numGLverts];
   memcpy(vertexes, oldVerts, sizeof(vertex_t) * numvertexes);
   delete[] oldVerts;

   // read the new verts in
   for (i = 0; i < numGLverts; i++)
   {
      if (isV2)
      {
         data >> vx >> vy;
         vertexes[i + numvertexes].x = (fixed_t)vx;
         vertexes[i + numvertexes].y = (fixed_t)vy;
      }
      else
      {
         data >> mx >> my;
         vertexes[i + numvertexes].x = (fixed_t)(mx << FRACBITS);
         vertexes[i + numvertexes].y = (fixed_t)(my << FRACBITS);
      }
   }

   numvertexes += numGLverts;

   Printf( PRINT_OPENGL, "added %d vertices.\n", numGLverts);
}


void P_LoadGLSegs(int lumpnum)
{
   int  i;
	FMemLump lumpdata;
	const BYTE *data;
	BYTE *vertchanged = new BYTE[numvertexes];	// phares 10/4/98
	DWORD segangle;
	line_t* line;		// phares 10/4/98
	int ptp_angle;		// phares 10/4/98
	int delta_angle;	// phares 10/4/98
	int dis;			// phares 10/4/98
	int dx,dy;			// phares 10/4/98
	int vnum1,vnum2;	// phares 10/4/98
   int firstGLvertex = numvertexes - numGLverts;

	memset (vertchanged,0,numvertexes); // phares 10/4/98

	numsegs = Wads.LumpLength (lumpnum) / sizeof(glseg_t);

	segs = new seg_t[numsegs];
	memset (segs, 0, numsegs*sizeof(seg_t));
	lumpdata = Wads.ReadLump (lumpnum);
	data = (const BYTE *)lumpdata.GetMem();

	line = lines;
	for (i = 0; i < numlines ; i++, line++)
	{
		vertchanged[line->v1 - vertexes] = vertchanged[line->v2 - vertexes] = 1;
	}

	for (i = 0; i < numsegs; i++)
	{
		seg_t *li = segs+i;
		glseg_t *gls = (glseg_t *) data + i;

		int side, linedef;
		line_t *ldef;

      li->v1 = &vertexes[gls->startVert & 0x8000 ? firstGLvertex + (gls->startVert & ~0x8000) : gls->startVert];
		li->v2 = &vertexes[gls->endVert & 0x8000 ? firstGLvertex + (gls->endVert & ~0x8000) : gls->endVert];
      if (gls->lineDef != 0xFFFF)
      {
         ldef = &lines[gls->lineDef];
         if (gls->partnerSeg != 0xFFFF)
         {
            li->PartnerSeg = &segs[gls->partnerSeg];
         }
         else
         {
			   li->PartnerSeg = NULL;
         }

         segangle = R_PointToAngle2(ldef->v1->x, ldef->v1->y, ldef->v2->x, ldef->v2->y);
         if (gls->sideNum)
         {
            segangle -= ANGLE_180;
         }

			ptp_angle = R_PointToAngle2 (li->v1->x, li->v1->y, li->v2->x, li->v2->y);
			dis = 0;
			delta_angle = (abs(ptp_angle-(segangle<<16))>>ANGLETOFINESHIFT)*360/FINEANGLES;

			vnum1 = li->v1 - vertexes;
			vnum2 = li->v2 - vertexes;

			if (vnum1 >= numvertexes || vnum2 >= numvertexes)
			{
				throw i * 4;
			}

			if (delta_angle != 0)
			{
				segangle >>= ANGLETOFINESHIFT;
				dx = (li->v1->x - li->v2->x)>>FRACBITS;
				dy = (li->v1->y - li->v2->y)>>FRACBITS;
				dis = ((int)sqrt((double)(dx*dx + dy*dy)))<<FRACBITS;
				dx = finecosine[segangle];
				dy = finesine[segangle];
				if ((vnum2 > vnum1) && (vertchanged[vnum2] == 0))
				{
					li->v2->x = li->v1->x + FixedMul(dis,dx);
					li->v2->y = li->v1->y + FixedMul(dis,dy);
					vertchanged[vnum2] = 1; // this was changed
				}
				else if (vertchanged[vnum1] == 0)
				{
					li->v1->x = li->v2->x - FixedMul(dis,dx);
					li->v1->y = li->v2->y - FixedMul(dis,dy);
					vertchanged[vnum1] = 1; // this was changed
				}
			}

			linedef = SHORT(gls->lineDef);
			ldef = &lines[linedef];
			li->linedef = ldef;
			side = SHORT(gls->sideNum);
			li->sidedef = &sides[ldef->sidenum[side]];
			li->frontsector = sides[ldef->sidenum[side]].sector;

			// killough 5/3/98: ignore 2s flag if second sidedef missing:
			if (ldef->flags & ML_TWOSIDED && ldef->sidenum[side^1] != NO_INDEX)
			{
				li->backsector = sides[ldef->sidenum[side^1]].sector;
			}
			else
			{
				li->backsector = 0;
				ldef->flags &= ~ML_TWOSIDED;
			}
      }
      else
      {
         // miniseg!
         li->frontsector = NULL;
         li->backsector = NULL;
         li->sidedef = NULL;
         li->linedef = NULL;
      }
	}

	delete[] vertchanged; // phares 10/4/98

   Printf("ZGL: read %d segs.\n", numsegs);
}


void P_LoadGLSubsectors(int lumpnum)
{
	FWadLump data;
	int i;

	numsubsectors = Wads.LumpLength (lumpnum) / sizeof(mapsubsector_t);

	subsectors = new subsector_t[numsubsectors];		
	data = Wads.OpenLumpNum (lumpnum);
	memset (subsectors, 0, numsubsectors * sizeof(subsector_t));
	
	for (i = 0; i < numsubsectors; i++)
	{
		WORD numsegs, firstseg;

		data >> numsegs >> firstseg;

		subsectors[i].numlines = numsegs;
		subsectors[i].firstline = firstseg;
	}

   Printf( PRINT_OPENGL, "read %d subsectors.\n", numsubsectors);
}

/*
void P_LoadGLNodes(int lumpnum)
{
}
*/
