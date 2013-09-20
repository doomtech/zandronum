#ifndef __GL_CLIPPER
#define __GL_CLIPPER

#include "tables.h"

class ClipNode
{
	friend class Clipper;
	friend class ClipNodesFreer;
	
	ClipNode *prev, *next;
	angle_t start, end;
	static ClipNode * freelist;

	bool operator== (const ClipNode &other)
	{
		return other.start == start && other.end == end;
	}

	void Free()
	{
		next=freelist;
		freelist=this;
	}

	static ClipNode * GetNew()
	{
		if (freelist)
		{
			ClipNode * p=freelist;
			freelist=p->next;
			return p;
		}
		else return new ClipNode;
	}

	static ClipNode * NewRange(angle_t start, angle_t end)
	{
		ClipNode * c=GetNew();

		c->start=start;
		c->end=end;
		c->next=c->prev=NULL;
		return c;
	}

};


class Clipper
{
	ClipNode * clipnodes;
	ClipNode * cliphead;

	void RemoveRange(ClipNode * cn);

public:

	static int anglecache;

	Clipper()
	{
		clipnodes=cliphead=NULL;
	}

	~Clipper();

	void Clear();

	bool IsRangeVisible(angle_t startangle, angle_t endangle);

	bool SafeCheckRange(angle_t startAngle, angle_t endAngle)
	{
		if(startAngle > endAngle)
		{
			return (IsRangeVisible(startAngle, ANGLE_MAX) || IsRangeVisible(0, endAngle));
		}
		
		return IsRangeVisible(startAngle, endAngle);
	}

	void AddClipRange(angle_t startangle, angle_t endangle);
	void SafeAddClipRange(angle_t startangle, angle_t endangle)
	{
		if(startangle > endangle)
		{
			// The range has to added in two parts.
			AddClipRange(startangle, ANGLE_MAX);
			AddClipRange(0, endangle);
		}
		else
		{
			// Add the range as usual.
			AddClipRange(startangle, endangle);
		}
	}

	void RemoveClipRange(angle_t startangle, angle_t endangle);
	void SafeRemoveClipRange(angle_t startangle, angle_t endangle)
	{
		if(startangle > endangle)
		{
			// The range has to added in two parts.
			RemoveClipRange(startangle, ANGLE_MAX);
			RemoveClipRange(0, endangle);
		}
		else
		{
			// Add the range as usual.
			RemoveClipRange(startangle, endangle);
		}
	}


	bool CheckBox(const fixed_t *bspcoord);
};


extern Clipper clipper;


// Used to speed up angle calculations during clipping
inline angle_t vertex_t::GetViewAngle()
{
	return angletime == Clipper::anglecache? viewangle : (angletime = Clipper::anglecache, viewangle = R_PointToAngle2(viewx, viewy, x,y));
}

#endif
