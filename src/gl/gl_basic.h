#ifndef __GL_BASIC
#define __GL_BASIC

#include "stats.h"

// Basic data structures used by the GL renderer

struct Clock
{
private:
	cycle_t counter;
	cycle_t accum;

public:
	Clock() { accum=counter=0; }

	void Reset() { counter=accum=0; }
	void Start(bool sub=false) 
	{ 
		counter=GetClockCycle(); 
	}
	void Stop(bool sub=false) 
	{ 
		accum+=GetClockCycle()-counter; 
	}

	cycle_t LastCycle() { return GetClockCycle()-counter; }
	cycle_t operator *() { return accum; }
};

extern Clock RenderWall,SetupWall,ClipWall;
extern Clock RenderFlat,SetupFlat;
extern Clock RenderSprite,SetupSprite;
extern Clock All, Finish, PortalAll;
extern int vertexcount, flatvertices, flatprimitives;


template <class T> struct FreeList
{
	T * freelist;

	T * GetNew()
	{
		if (freelist)
		{
			T * n=freelist;
			freelist=*((T**)n);
			return n;
		}
		return new T;
	}

	void Release(T * node)
	{
		*((T**)node) = freelist;
		freelist=node;
	}

	~FreeList()
	{
		while (freelist!=NULL)
		{
			T * n = freelist;
			freelist=*((T**)n);
			delete n;
		}
	}
};

template<class T> class UniqueList
{
	TArray<T*> Array;
	FreeList<T>	FreeList;

public:

	T * Get(T * t)
	{
		for(unsigned i=0;i<Array.Size();i++)
		{
			if (!memcmp(t, Array[i], sizeof(T))) return Array[i];
		}
		T * newo=FreeList.GetNew();

		*newo=*t;
		Array.Push(newo);
		return newo;
	}

	void Clear()
	{
		for(unsigned i=0;i<Array.Size();i++) FreeList.Release(Array[i]);
		Array.Clear();
	}

	~UniqueList()
	{
		Clear();
	}
};

#endif
