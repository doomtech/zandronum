#include "dobject.h"
#include "i_system.h"
#include "actor.h"
#include "autosegs.h"
#include "templates.h"

TArray<PClass *> PClass::m_RuntimeActors;
TArray<PClass *> PClass::m_Types;
PClass *PClass::TypeHash[PClass::HASH_SIZE];

// A harmless non_NULL FlatPointer for classes without pointers.
static const size_t TheEnd = ~0;

static int STACK_ARGS cregcmp (const void *a, const void *b)
{
	// VC++ introduces NULLs in the sequence. GCC seems to work as expected and not do it.
	const ClassReg *class1 = *(const ClassReg **)a;
	const ClassReg *class2 = *(const ClassReg **)b;
	if (class1 == NULL) return 1;
	if (class2 == NULL) return -1;
	return strcmp (class1->Name, class2->Name);
}

void PClass::StaticInit ()
{
	atterm (StaticShutdown);

	// Sort classes by name to remove dependance on how the compiler ordered them.
	REGINFO *head = &CRegHead;
	REGINFO *tail = &CRegTail;

	// MinGW's linker is linking the object files backwards for me now...
	if (head > tail)
	{
		swap (head, tail);
	}
	qsort (head + 1, tail - head - 1, sizeof(REGINFO), cregcmp);

	TAutoSegIterator<ClassReg *, &CRegHead, &CRegTail> probe;

	while (++probe != NULL)
	{
		probe->RegisterClass ();
	}
}

void PClass::StaticShutdown ()
{
	TArray<size_t *> uniqueFPs(64);
	unsigned int i, j;

	for (i = 0; i < PClass::m_Types.Size(); ++i)
	{
		PClass *type = PClass::m_Types[i];
		PClass::m_Types[i] = NULL;
		if (type->FlatPointers != &TheEnd && type->FlatPointers != type->Pointers)
		{
			// FlatPointers are shared by many classes, so we must check for
			// duplicates and only delete those that are unique.
			for (j = 0; j < uniqueFPs.Size(); ++j)
			{
				if (type->FlatPointers == uniqueFPs[j])
				{
					break;
				}
			}
			if (j == uniqueFPs.Size())
			{
				uniqueFPs.Push(const_cast<size_t *>(type->FlatPointers));
			}
		}
		// For runtime classes, this call will also delete the PClass.
		PClass::StaticFreeData (type);
	}
	for (i = 0; i < uniqueFPs.Size(); ++i)
	{
		delete[] uniqueFPs[i];
	}
}

void PClass::StaticFreeData (PClass *type)
{
	if (type->Defaults != NULL)
	{
		delete[] type->Defaults;
		type->Defaults = NULL;
	}
	if (type->bRuntimeClass)
	{
		if (type->ActorInfo != NULL)
		{
			if (type->ActorInfo->OwnedStates != NULL)
			{
				delete[] type->ActorInfo->OwnedStates;
				type->ActorInfo->OwnedStates = NULL;
			}
			delete type->ActorInfo;
			type->ActorInfo = NULL;
		}
		delete type;
	}
}

void ClassReg::RegisterClass ()
{
	assert (MyClass != NULL);

	// Add type to list
	MyClass->ClassIndex = PClass::m_Types.Push (MyClass);

	MyClass->TypeName = FName(Name+1);
	MyClass->ParentClass = ParentType;
	MyClass->Size = SizeOf;
	MyClass->Pointers = Pointers;
	MyClass->ConstructNative = ConstructNative;
	MyClass->InsertIntoHash ();
}

void PClass::InsertIntoHash ()
{
	// Add class to hash table. Classes are inserted into each bucket
	// in ascending order by name index.
	unsigned int bucket = TypeName % HASH_SIZE;
	PClass **hashpos = &TypeHash[bucket];
	while (*hashpos != NULL)
	{
		int lexx = int(TypeName) - int((*hashpos)->TypeName);

		if (lexx > 0)
		{ // This type should come later in the chain
			hashpos = &((*hashpos)->HashNext);
		}
		else if (lexx == 0)
		{ // This type has already been inserted
			I_FatalError ("Tried to register class '%s' more than once.", TypeName.GetChars());
		}
		else
		{ // Type comes right here
			break;
		}
	}
	HashNext = *hashpos;
	*hashpos = this;
}

// Find a type, passed the name as a string
const PClass *PClass::FindClass (const char *zaname)
{
	return FindClass (FName (zaname, true));
}

// Find a type, passed the name as a name
const PClass *PClass::FindClass (FName zaname)
{
	if (zaname == NAME_None)
	{
		return NULL;
	}

	PClass *cls = TypeHash[zaname % HASH_SIZE];

	while (cls != 0)
	{
		int lexx = int(zaname) - int(cls->TypeName);
		if (lexx > 0)
		{
			cls = cls->HashNext;
		}
		else if (lexx == 0)
		{
			return cls;
		}
		else
		{
			break;
		}
	}
	return NULL;
}

// Create a new object that this class represents
DObject *PClass::CreateNew () const
{
	BYTE *mem = (BYTE *)M_Malloc (Size);
	assert (mem != NULL);

	// Set this object's defaults before constructing it.
	if (Defaults!=NULL)
		memcpy (mem, Defaults, Size);
	else
		memset (mem, 0, Size);

	ConstructNative (mem);
	((DObject *)mem)->SetClass (const_cast<PClass *>(this));
	return (DObject *)mem;
}

// Create a new class based on an existing class
PClass *PClass::CreateDerivedClass (FName name, unsigned int size)
{
	assert (size >= Size);

	PClass *type = new PClass;

	type->TypeName = name;
	type->ParentClass = this;
	type->Size = size;
	type->Pointers = NULL;
	type->ConstructNative = ConstructNative;
	type->ClassIndex = m_Types.Push (type);
	type->Meta = Meta;
	type->Defaults = new BYTE[size];
	memcpy (type->Defaults, Defaults, Size);
	if (size > Size)
	{
		memset (type->Defaults + Size, 0, size - Size);
	}
	type->FlatPointers = NULL;
	type->bRuntimeClass = true;
	type->ActorInfo = NULL;
	type->InsertIntoHash();

	// If this class has an actor info, then any classes derived from it
	// also need an actor info.
	if (this->ActorInfo != NULL)
	{
		FActorInfo *info = type->ActorInfo = new FActorInfo;
		info->Class = type;
		info->GameFilter = GAME_Any;
		info->SpawnID = 0;
		info->DoomEdNum = -1;
		info->OwnedStates = NULL;
		info->NumOwnedStates = 0;
		info->Replacement = NULL;
		info->Replacee = NULL;
		m_RuntimeActors.Push (type);
	}
	return type;
}

// Create the FlatPointers array, if it doesn't exist already.
// It comprises all the Pointers from superclasses plus this class's own Pointers.
// If this class does not define any new Pointers, then FlatPointers will be set
// to the same array as the super class's.
void PClass::BuildFlatPointers ()
{
	if (FlatPointers != NULL)
	{ // Already built: Do nothing.
		return;
	}
	else if (ParentClass == NULL)
	{ // No parent: FlatPointers is the same as Pointers.
		if (Pointers == NULL)
		{ // No pointers: Make FlatPointers a harmless non-NULL.
			FlatPointers = &TheEnd;
		}
		else
		{
			FlatPointers = Pointers;
		}
	}
	else
	{
		ParentClass->BuildFlatPointers ();
		if (Pointers == NULL)
		{ // No new pointers: Just use the same FlatPointers as the parent.
			FlatPointers = ParentClass->FlatPointers;
		}
		else
		{ // New pointers: Create a new FlatPointers array and add them.
			int numPointers, numSuperPointers;

			// Count pointers defined by this class.
			for (numPointers = 0; Pointers[numPointers] != ~(size_t)0; numPointers++)
			{ }
			// Count pointers defined by superclasses.
			for (numSuperPointers = 0; ParentClass->FlatPointers[numSuperPointers] != ~(size_t)0; numSuperPointers++)
			{ }

			// Concatenate them into a new array
			size_t *flat = new size_t[numPointers + numSuperPointers + 1];
			if (numSuperPointers > 0)
			{
				memcpy (flat, ParentClass->FlatPointers, sizeof(size_t)*numSuperPointers);
			}
			memcpy (flat + numSuperPointers, Pointers, sizeof(size_t)*(numPointers+1));
			FlatPointers = flat;
		}
	}
}
