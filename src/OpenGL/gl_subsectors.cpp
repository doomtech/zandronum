
#include "gl_main.h"
#include "tarray.h"
#include "templates.h"


VisibleSubsector *sortedsubsectors;


SubsectorList::SubsectorList() : Head(0), Tail(0)
{
}


SubsectorList::~SubsectorList()
{
   Clear();
}


void SubsectorList::Clear()
{
   if (Head) delete Head;
   Head = NULL;
   Tail = NULL;
}


void SubsectorList::AddSubsector(subsector_t *subSec)
{
   SubsectorList::SubsectorListNode *node = new SubsectorList::SubsectorListNode();

   node->Subsec = subSec;

   if (Tail != NULL)
   {
      Tail->Next = node;
      Tail = Tail->Next;
   }
   else
   {
      Head = Tail = node;
   }
}


void SubsectorList::UnlinkList()
{
   Head = Tail = NULL;
}


void SubsectorList::LinkList(SubsectorList::SubsectorListNode *node)
{
   if (Tail)
   {
      Tail->Next = node;
   }
   else
   {
      Head = Tail = node;
   }

   while (Tail->Next) Tail = Tail->Next;
}


SubsectorList::SubsectorListNode::SubsectorListNode() : Subsec(0), Next(0)
{
}


SubsectorList::SubsectorListNode::~SubsectorListNode()
{
   if (Next)
   {
      delete Next;
      Next = NULL;
   }
}


void GL_ClipSubsector(subsector_t *subSec, sector_t *sector)
{
   int numLines, firstLine, i;
   seg_t *seg;
   sector_t tempsec;

   if (subSec->isPoly) return;

   if (subSec->poly)
   {
      numLines = subSec->poly->numsegs;
      for (i = 0; i < numLines; i++)
      {
         seg = subSec->poly->segs[i];
         if (CL_ShouldClipAgainstSeg(seg, sector, R_FakeFlat(seg->backsector, &tempsec, NULL, NULL, false)))
         {
            CL_AddSegRange(seg);
         }
      }
   }

   firstLine = subSec->firstline;
   numLines = subSec->numlines + firstLine;

   for (i = firstLine; i < numLines; i++)
   {
      seg = segs + i;
      if (CL_ShouldClipAgainstSeg(seg, sector, R_FakeFlat(seg->backsector, &tempsec, NULL, NULL, false)))
      {
         CL_AddSegRange(seg);
      }
   }
}


//
// by Matt Whitlock, May 2003
// adapted for use with ZDoomGL.  Apparently this is faster than the libcrt version.
//
void GL_subsecQsort(VisibleSubsector *left, VisibleSubsector *right)
{
	VisibleSubsector *leftbound, *rightbound, pivot, temp;

   pivot = left[(right - left) / (2 * sizeof(VisibleSubsector))];
   leftbound = left;
   rightbound = right;
   for (;;)
   {
      while (leftbound->dist < pivot.dist)
      {
         leftbound++;
      }
      while (rightbound->dist > pivot.dist)
      {
         rightbound--;
      }
      while ((leftbound != rightbound) && (leftbound->dist == rightbound->dist))
      {
         leftbound++;
      }
      if (leftbound == rightbound)
      {
         break;
      }
      temp = *leftbound;
      *leftbound = *rightbound;
      *rightbound = temp;
   }
   if (left < --leftbound)
   {
      GL_subsecQsort(left, leftbound);
   }
   if (right > ++rightbound)
   {
      GL_subsecQsort(rightbound, right);
   }
}


//
// sort all subsectors by distance from viewer
//  note that this should really be optimized somehow.  maps with lots of subsectors tend to slow down a bit :(
//
void GL_SortSubsectors()
{
   int i;

   // just using the center point of the subsector to sort against isn't the ideal solution (long, skinny subsectors can have skewed results), but
   // it'll have to do for now
   for (i = 0; i < numsubsectors; i++)
   {
      sortedsubsectors[i].dist = R_PointToDist2(sortedsubsectors[i].subSec->CenterX, sortedsubsectors[i].subSec->CenterY);
   }

   GL_subsecQsort(sortedsubsectors, sortedsubsectors + numsubsectors - 1);
}

