/*
*
** gl_clipper.cpp
**
** Handles visibility checks.
** Loosely based on the JDoom clipper.
**
**---------------------------------------------------------------------------
** Copyright 2003 Tim Stump
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "gl/gl_include.h"
#include "r_main.h"
#include "gl/gl_clipper.h"
#include "gl/gl_values.h"



ClipNode * ClipNode::freelist;


//-----------------------------------------------------------------------------
//
// Destructor
//
//-----------------------------------------------------------------------------
Clipper::~Clipper()
{
	Clear();
	while (ClipNode::freelist != NULL)
	{
		ClipNode * node = ClipNode::freelist;
		ClipNode::freelist = node->next;
		delete node;
	}
}

//-----------------------------------------------------------------------------
//
// RemoveRange
//
//-----------------------------------------------------------------------------
void Clipper::RemoveRange(ClipNode * range)
{
	if (range == cliphead)
	{
		cliphead = cliphead->next;
	}
	else
	{
		if (range->prev) range->prev->next = range->next;
		if (range->next) range->next->prev = range->prev;
	}
	
	range->Free();
}

//-----------------------------------------------------------------------------
//
// Clear
//
//-----------------------------------------------------------------------------
void Clipper::Clear()
{
	ClipNode *node = cliphead;
	ClipNode *temp;
	
	while (node != NULL)
	{
		temp = node;
		node = node->next;
		temp->Free();
	}
	
	cliphead = NULL;
}


//-----------------------------------------------------------------------------
//
// IsRangeVisible
//
//-----------------------------------------------------------------------------
bool Clipper::IsRangeVisible(angle_t startAngle, angle_t endAngle)
{
	ClipNode *ci;
	ci = cliphead;
	
	if (endAngle==0 && ci && ci->start==0) return false;
	
	while (ci != NULL && ci->start < endAngle)
	{
		if (startAngle >= ci->start && endAngle <= ci->end)
		{
			return false;
		}
		ci = ci->next;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
//
// AddClipRange
//
//-----------------------------------------------------------------------------
void Clipper::AddClipRange(angle_t start, angle_t end)
{
	ClipNode *node, *temp, *prevNode;

	if (cliphead)
	{
		//check to see if range contains any old ranges
		node = cliphead;
		while (node != NULL && node->start < end)
		{
			if (node->start >= start && node->end <= end)
			{
				temp = node;
				node = node->next;
				RemoveRange(temp);
			}
			else if (node->start<=start && node->end>=end)
			{
				return;
			}
			else
			{
				node = node->next;
			}
		}
		
		//check to see if range overlaps a range (or possibly 2)
		node = cliphead;
		while (node != NULL)
		{
			if (node->start >= start && node->start <= end)
			{
				node->start = start;
				return;
			}
			
			if (node->end >= start && node->end <= end)
			{
				// check for possible merger
				if (node->next && node->next->start <= end)
				{
					node->end = node->next->end;
					RemoveRange(node->next);
				}
				else
				{
					node->end = end;
				}
				
				return;
			}
			
			node = node->next;
		}
		
		//just add range
		node = cliphead;
		prevNode = NULL;
		temp = ClipNode::NewRange(start, end);
		
		while (node != NULL && node->start < end)
		{
			prevNode = node;
			node = node->next;
		}
		
		temp->next = node;
		if (node == NULL)
		{
			temp->prev = prevNode;
			if (prevNode) prevNode->next = temp;
			if (!cliphead) cliphead = temp;
		}
		else
		{
			if (node == cliphead)
			{
				cliphead->prev = temp;
				cliphead = temp;
			}
			else
			{
				temp->prev = prevNode;
				prevNode->next = temp;
				node->prev = temp;
			}
		}
	}
	else
	{
		temp = ClipNode::NewRange(start, end);
		cliphead = temp;
		return;
	}
}


//-----------------------------------------------------------------------------
//
// RemoveClipRange
//
//-----------------------------------------------------------------------------
void Clipper::RemoveClipRange(angle_t start, angle_t end)
{
	ClipNode *node, *temp;
	
	if (cliphead)
	{
		//check to see if range contains any old ranges
		node = cliphead;
		while (node != NULL && node->start < end)
		{
			if (node->start >= start && node->end <= end)
			{
				temp = node;
				node = node->next;
				RemoveRange(temp);
			}
			else
			{
				node = node->next;
			}
		}
		
		//check to see if range overlaps a range (or possibly 2)
		node = cliphead;
		while (node != NULL)
		{
			if (node->start >= start && node->start <= end)
			{
				node->start = end;
				break;
			}
			else if (node->end >= start && node->end <= end)
			{
				node->end=start;
			}
			else if (node->start < start && node->end > end)
			{
				temp=ClipNode::NewRange(end, node->end);
				node->end=start;
				temp->next=node->next;
				temp->prev=node;
				node->next=temp;
				if (temp->next) temp->next->prev=temp;
				break;
			}
			node = node->next;
		}
	}
}




//-----------------------------------------------------------------------------
//
// R_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true
//  if some part of the bbox might be visible.
//
//-----------------------------------------------------------------------------

bool Clipper::CheckBox(const fixed_t *bspcoord) 
{
	static const int checkcoord[12][4] = // killough -- static const
	{
	  {3,0,2,1},
	  {3,0,2,0},
	  {3,1,2,0},
	  {0},
	  {2,0,2,1},
	  {0,0,0,0},
	  {3,1,3,0},
	  {0},
	  {2,0,3,1},
	  {2,1,3,1},
	  {2,1,3,0}
	};


	angle_t angle1, angle2;

	int        boxpos;
	const int* check;
	
	// Find the corners of the box
	// that define the edges from current viewpoint.
	boxpos = (viewx <= bspcoord[BOXLEFT] ? 0 : viewx < bspcoord[BOXRIGHT ] ? 1 : 2) +
		(viewy >= bspcoord[BOXTOP ] ? 0 : viewy > bspcoord[BOXBOTTOM] ? 4 : 8);
	
	if (boxpos == 5) return true;
	
	check = checkcoord[boxpos];
	angle1 = R_PointToAngle (bspcoord[check[0]], bspcoord[check[1]]);
	angle2 = R_PointToAngle (bspcoord[check[2]], bspcoord[check[3]]);
	
	return SafeCheckRange(angle2, angle1);
}

