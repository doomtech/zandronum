#ifndef __GL_BASIC
#define __GL_BASIC

#include "stats.h"

class glcycle_t
{
public:
	glcycle_t &operator= (const glcycle_t &o)
	{
		Counter = o.Counter;
		return *this;
	}

	void Reset()
	{
		Counter = 0;
	}
	
	void Clock()
	{
		// Not using QueryPerformanceCounter directly, so we don't need
		// to pull in the Windows headers for every single file that
		// wants to do some profiling.
		//long long time = QueryPerfCounter();
		//Counter -= time;
	}
	
	void Unclock()
	{
		//long long time = QueryPerfCounter();
		//Counter += time;
	}
	
	double Time()
	{
		return 0; //Counter * GLPerfToSec;
	}
	
	double TimeMS()
	{
		return 0; //Counter * GLPerfToMillisec;
	}

private:
	long long Counter;
};


extern glcycle_t RenderWall,SetupWall,ClipWall;
extern glcycle_t RenderFlat,SetupFlat;
extern glcycle_t RenderSprite,SetupSprite;
extern glcycle_t All, Finish, PortalAll;
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
	FreeList<T>	Freelist;

public:

	T * Get(T * t)
	{
		for(unsigned i=0;i<Array.Size();i++)
		{
			if (!memcmp(t, Array[i], sizeof(T))) return Array[i];
		}
		T * newo=Freelist.GetNew();

		*newo=*t;
		Array.Push(newo);
		return newo;
	}

	void Clear()
	{
		for(unsigned i=0;i<Array.Size();i++) Freelist.Release(Array[i]);
		Array.Clear();
	}

	~UniqueList()
	{
		Clear();
	}
};

#endif
