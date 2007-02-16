/*
** farchive.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#ifndef __FARCHIVE_H__
#define __FARCHIVE_H__

#include <stdio.h>
#include "doomtype.h"
#include "dobject.h"
#include "tarray.h"
#include "name.h"

class FFile
{
public:
		enum EOpenMode
		{
			EReading,
			EWriting,
			ENotOpen
		};

		enum ESeekPos
		{
			ESeekSet,
			ESeekRelative,
			ESeekEnd
		};

virtual	~FFile () {}

virtual	bool Open (const char *name, EOpenMode mode) = 0;
virtual	void Close () = 0;
virtual	void Flush () = 0;
virtual EOpenMode Mode () const = 0;
virtual bool IsPersistent () const = 0;
virtual bool IsOpen () const = 0;

virtual	FFile& Write (const void *, unsigned int) = 0;
virtual	FFile& Read (void *, unsigned int) = 0;

virtual	unsigned int Tell () const = 0;
virtual	FFile& Seek (int, ESeekPos) = 0;
inline	FFile& Seek (unsigned int i, ESeekPos p) { return Seek ((int)i, p); }
};

class FCompressedFile : public FFile
{
public:
	FCompressedFile ();
	FCompressedFile (const char *name, EOpenMode mode, bool dontcompress = false);
	FCompressedFile (FILE *file, EOpenMode mode, bool dontcompress = false, bool postopen=true);
	~FCompressedFile ();

	bool Open (const char *name, EOpenMode mode);
	void Close ();
	void Flush ();
	EOpenMode Mode () const;
	bool IsPersistent () const { return true; }
	bool IsOpen () const;
	unsigned int GetSize () const { return m_BufferSize; }

	FFile &Write (const void *, unsigned int);
	FFile &Read (void *, unsigned int);
	unsigned int Tell () const;
	FFile &Seek (int, ESeekPos);

protected:
	unsigned int m_Pos;
	unsigned int m_BufferSize;
	unsigned int m_MaxBufferSize;
	unsigned char *m_Buffer;
	bool m_NoCompress;
	EOpenMode m_Mode;
	FILE *m_File;

	void Implode ();
	void Explode ();
	virtual bool FreeOnExplode () { return true; }
	void PostOpen ();

private:
	void BeEmpty ();
};

class FCompressedMemFile : public FCompressedFile
{
public:
	FCompressedMemFile ();
	FCompressedMemFile (FILE *file);	// Create for reading
	~FCompressedMemFile ();

	bool Open (const char *name, EOpenMode mode);	// Works for reading only
	bool Open (void *memblock);	// Open for reading only
	bool Open ();	// Open for writing only
	bool Reopen ();	// Re-opens imploded file for reading only
	void Close ();
	bool IsOpen () const;

	void Serialize (FArchive &arc);

protected:
	bool FreeOnExplode () { return !m_SourceFromMem; }

private:
	bool m_SourceFromMem;
	unsigned char *m_ImplodedBuffer;
};

class FPNGChunkFile : public FCompressedFile
{
public:
	FPNGChunkFile (FILE *file, DWORD id);					// Create for writing
	FPNGChunkFile (FILE *file, DWORD id, size_t chunklen);	// Create for reading

	void Close ();

private:
	DWORD m_ChunkID;
};

class FArchive
{
public:
		FArchive (FFile &file);
		virtual ~FArchive ();

		inline bool IsLoading () const { return m_Loading; }
		inline bool IsStoring () const { return m_Storing; }
		inline bool IsPeristent () const { return m_Persistent; }
		
		void SetHubTravel () { m_HubTravel = true; }

		void Close ();

virtual	void Write (const void *mem, unsigned int len);
virtual void Read (void *mem, unsigned int len);

		void WriteString (const char *str);
		void WriteCount (DWORD count);
		DWORD ReadCount ();

		void UserWriteClass (const PClass *info);
		void UserReadClass (const PClass *&info);

		FArchive& operator<< (BYTE &c);
		FArchive& operator<< (WORD &s);
		FArchive& operator<< (DWORD &i);
		FArchive& operator<< (QWORD &i);
		FArchive& operator<< (float &f);
		FArchive& operator<< (double &d);
		FArchive& operator<< (char *&str);
		FArchive& operator<< (FName &n);
		FArchive& operator<< (FString &str);
		FArchive& SerializePointer (void *ptrbase, BYTE **ptr, DWORD elemSize);
		FArchive& SerializeObject (DObject *&object, PClass *type);
		FArchive& WriteObject (DObject *obj);
		FArchive& ReadObject (DObject *&obj, PClass *wanttype);

		void WriteName (const char *name);
		const char *ReadName ();	// The returned name disappears with the archive, unlike strings

		void WriteSprite (int spritenum);
		int ReadSprite ();

inline	FArchive& operator<< (SBYTE &c) { return operator<< ((BYTE &)c); }
inline	FArchive& operator<< (SWORD &s) { return operator<< ((WORD &)s); }
inline	FArchive& operator<< (SDWORD &i) { return operator<< ((DWORD &)i); }
inline	FArchive& operator<< (SQWORD &i) { return operator<< ((QWORD &)i); }
inline	FArchive& operator<< (unsigned char *&str) { return operator<< ((char *&)str); }
inline	FArchive& operator<< (signed char *&str) { return operator<< ((char *&)str); }
inline	FArchive& operator<< (bool &b) { return operator<< ((BYTE &)b); }
inline  FArchive& operator<< (DObject* &object) { return ReadObject (object, RUNTIME_CLASS(DObject)); }

protected:
		enum { EObjectHashSize = 137 };

		DWORD FindObjectIndex (const DObject *obj) const;
		DWORD MapObject (const DObject *obj);
		DWORD WriteClass (const PClass *info);
		const PClass *ReadClass ();
		const PClass *ReadClass (const PClass *wanttype);
		const PClass *ReadStoredClass (const PClass *wanttype);
		DWORD HashObject (const DObject *obj) const;
		DWORD AddName (const char *name);
		DWORD AddName (unsigned int start);	// Name has already been added to storage
		DWORD FindName (const char *name) const;
		DWORD FindName (const char *name, unsigned int bucket) const;

		bool m_Persistent;		// meant for persistent storage (disk)?
		bool m_Loading;			// extracting objects?
		bool m_Storing;			// inserting objects?
		bool m_HubTravel;		// travelling inside a hub?
		FFile *m_File;			// unerlying file object
		DWORD m_ObjectCount;	// # of objects currently serialized
		DWORD m_MaxObjectCount;
		DWORD m_ClassCount;		// # of unique classes currently serialized

		struct TypeMap
		{
			const PClass *toCurrent;	// maps archive type index to execution type index
			DWORD toArchive;		// maps execution type index to archive type index

			enum { NO_INDEX = 0xffffffff };
		} *m_TypeMap;

		struct ObjectMap
		{
			const DObject *object;
			size_t hashNext;
		} *m_ObjectMap;
		size_t m_ObjectHash[EObjectHashSize];

		struct NameMap
		{
			DWORD StringStart;	// index into m_NameStorage
			DWORD HashNext;		// next in hash bucket
			enum { NO_INDEX = 0xffffffff };
		};
		TArray<NameMap> m_Names;
		TArray<char> m_NameStorage;
		unsigned int m_NameHash[EObjectHashSize];

		int *m_SpriteMap;
		size_t m_NumSprites;

		FArchive ();
		void AttachToFile (FFile &file);

private:
		FArchive (const FArchive &src) {}
		void operator= (const FArchive &src) {}
};

// Create an FPNGChunkFile and FArchive in one step
class FPNGChunkArchive : public FArchive
{
public:
	FPNGChunkArchive (FILE *file, DWORD chunkid);
	FPNGChunkArchive (FILE *file, DWORD chunkid, size_t chunklen);
	~FPNGChunkArchive ();
	FPNGChunkFile Chunk;
};

inline FArchive &operator<< (FArchive &arc, PalEntry &p)
{
	return arc << p.a << p.r << p.g << p.b;
}

#include "dobject.h"

template<class T>
inline FArchive &operator<< (FArchive &arc, T* &object)
{
	return arc.SerializeObject ((DObject*&)object, RUNTIME_CLASS(T));
}

FArchive &operator<< (FArchive &arc, const PClass * &info);

template<class T>
inline FArchive &operator<< (FArchive &arc, TArray<T> &self)
{
	unsigned int i;

	if (arc.IsStoring())
	{
		arc.WriteCount(self.Count);
	}
	else
	{
		DWORD numStored = arc.ReadCount();
		self.Resize(numStored);
	}
	for (i = 0; i < self.Count; ++i)
	{
		arc << self.Array[i];
	}
	return arc;
}

#endif //__FARCHIVE_H__
