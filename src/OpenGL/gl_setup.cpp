
#include "gl_main.h"
#include "w_wad.h"


typedef struct
{
	int x, y;
} glvert2_t;


typedef struct
{
   WORD startVert;
   WORD endVert;
   WORD lineDef;
   WORD sideNum;
   WORD partnerSeg;
} glseg2_t;

typedef struct
{
   DWORD startVert;
   DWORD endVert;
   WORD lineDef;
   WORD flags;
   DWORD partnerSeg;
} glseg3_t;

typedef struct
{
   DWORD numSegs;
   DWORD firstSeg;
} glssect3_t;

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

   Printf("ZGL: added %d vertices.\n", numGLverts);
}


void P_LoadGLSegs(int lumpnum)
{
   FWadLump data;
   DWORD id;
   glseg2_t gls2;
   glseg3_t gls3;
   bool isV3, validSeg;
   DWORD vertexBit, segangle;
   BYTE *vertchanged = new BYTE[numvertexes];
   int i, ptp_angle, delta_angle, dis, dx, dy;
   __w64 int vnum1, vnum2;
   line_t *line;
   int firstGLvertex = numvertexes - numGLverts;

   data = Wads.OpenLumpNum (lumpnum);
   data.Read(&id, 4);
   isV3 = id == MAKE_ID('g', 'N', 'd', '3');

   line = lines;
   memset (vertchanged,0,numvertexes);
   for (i = 0; i < numlines ; i++, line++)
	{
		vertchanged[line->v1 - vertexes] = vertchanged[line->v2 - vertexes] = 1;
	}

   if (isV3)
   {
      numsegs = (data.GetLength() - 4) / sizeof(gls3);
      vertexBit = 1 << 30;
   }
   else
   {
      data.Seek(0, SEEK_SET);
      numsegs = data.GetLength() / sizeof(gls2);
      vertexBit = 1 << 15;
   }

   segs = new seg_t[numsegs];
	memset (segs, 0, numsegs * sizeof(seg_t));

   for (i = 0; i < numsegs; i++)
   {
      seg_t *li = segs + i;
      line_t *ldef;
      int side, linedef;

      if (isV3)
      {
         data >> gls3.startVert >> gls3.endVert >> gls3.lineDef >> gls3.flags >> gls3.partnerSeg;
         li->v1 = &vertexes[gls3.startVert & vertexBit ? firstGLvertex + (gls3.startVert & ~vertexBit) : gls3.startVert];
         li->v2 = &vertexes[gls3.endVert & vertexBit ? firstGLvertex + (gls3.endVert & ~vertexBit) : gls3.endVert];
         validSeg = gls3.lineDef != 0xffff;
      }
      else
      {
         data >> gls2.startVert >> gls2.endVert >> gls2.lineDef >> gls2.sideNum >> gls2.partnerSeg;
         li->v1 = &vertexes[gls2.startVert & vertexBit ? firstGLvertex + (gls2.startVert & ~vertexBit) : gls2.startVert];
         li->v2 = &vertexes[gls2.endVert & vertexBit ? firstGLvertex + (gls2.endVert & ~vertexBit) : gls2.endVert];
         validSeg = gls2.lineDef != 0xffff;
      }

      if (validSeg)
      {
         if (isV3)
         {
            ldef = &lines[gls3.lineDef];
            if (gls3.partnerSeg != DWORD_MAX)
            {
               li->PartnerSeg = &segs[gls3.partnerSeg];
            }
            else
            {
			      li->PartnerSeg = NULL;
            }

            segangle = R_PointToAngle2(ldef->v1->x, ldef->v1->y, ldef->v2->x, ldef->v2->y);
            if (gls3.flags & 0x0001)
            {
               segangle -= ANGLE_180;
            }

            linedef = SHORT(gls3.lineDef);
            side = SHORT(gls3.flags & 0x0001);
         }
         else
         {
            ldef = &lines[gls2.lineDef];
            if (gls2.partnerSeg != 0xFFFF)
            {
               li->PartnerSeg = &segs[gls2.partnerSeg];
            }
            else
            {
			      li->PartnerSeg = NULL;
            }

            segangle = R_PointToAngle2(ldef->v1->x, ldef->v1->y, ldef->v2->x, ldef->v2->y);
            if (gls2.sideNum)
            {
               segangle -= ANGLE_180;
            }

            linedef = SHORT(gls2.lineDef);
            side = SHORT(gls2.sideNum);
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

			ldef = &lines[linedef];
			li->linedef = ldef;
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

   delete [] vertchanged;

   Printf("ZGL: read %d segs.\n", numsegs);
}


void P_LoadGLSubsectors(int lumpnum)
{
   FWadLump data;
   DWORD id;
   int i;

   data = Wads.OpenLumpNum (lumpnum);
   data.Read(&id, 4);

   if (id == MAKE_ID('g', 'N', 'd', '3'))
   {
      numsubsectors = Wads.LumpLength (lumpnum) / sizeof(glssect3_t);

      subsectors = new subsector_t[numsubsectors];
      memset (subsectors, 0, numsubsectors * sizeof(subsector_t));

      for (i = 0; i < numsubsectors; i++)
      {
         DWORD numsegs, firstseg;

         data >> numsegs >> firstseg;

         subsectors[i].numlines = numsegs;
         subsectors[i].firstline = firstseg;
      }
   }
   else
   {
      data.Seek(0, SEEK_SET);
	   numsubsectors = Wads.LumpLength (lumpnum) / sizeof(mapsubsector_t);

	   subsectors = new subsector_t[numsubsectors];
	   memset (subsectors, 0, numsubsectors * sizeof(subsector_t));

	   for (i = 0; i < numsubsectors; i++)
	   {
		   WORD numsegs, firstseg;

		   data >> numsegs >> firstseg;

		   subsectors[i].numlines = numsegs;
		   subsectors[i].firstline = firstseg;
	   }
   }

   Printf("ZGL: read %d subsectors.\n", numsubsectors);
}


void P_LoadGLNodes(int lumpnum)
{
}
