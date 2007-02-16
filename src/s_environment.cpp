#include "doomtype.h"
#include "tarray.h"
#include "s_sound.h"
#include "farchive.h"
#include "sc_man.h"
#include "cmdlib.h"
#include "templates.h"
#include "w_wad.h"
#include "i_system.h"

struct FEAXField
{
	int Min, Max;
	float REVERB_PROPERTIES::*Float;
	int REVERB_PROPERTIES::*Int;
	unsigned int Flag;
};

static const FEAXField EAXFields[] =
{
	{        0,       25, 0, &REVERB_PROPERTIES::Environment, 0 },
	{     1000,   100000, &REVERB_PROPERTIES::EnvSize, 0, 0 },
	{        0,     1000, &REVERB_PROPERTIES::EnvDiffusion, 0, 0 },
	{   -10000,        0, 0, &REVERB_PROPERTIES::Room, 0 },
	{   -10000,        0, 0, &REVERB_PROPERTIES::RoomHF, 0 },
	{   -10000,        0, 0, &REVERB_PROPERTIES::RoomLF, 0 },
	{      100,    20000, &REVERB_PROPERTIES::DecayTime, 0, 0 },
	{      100,     2000, &REVERB_PROPERTIES::DecayHFRatio, 0, 0 },
	{      100,     2000, &REVERB_PROPERTIES::DecayLFRatio, 0, 0 },
	{   -10000,     1000, 0, &REVERB_PROPERTIES::Reflections, 0 },
	{        0,      300, &REVERB_PROPERTIES::ReflectionsDelay, 0, 0 },
	{ -2000000,  2000000, &REVERB_PROPERTIES::ReflectionsPan0, 0, 0 },
	{ -2000000,  2000000, &REVERB_PROPERTIES::ReflectionsPan1, 0, 0 },
	{ -2000000,  2000000, &REVERB_PROPERTIES::ReflectionsPan2, 0, 0 },
	{   -10000,     2000, 0, &REVERB_PROPERTIES::Reverb, 0 },
	{        0,      100, &REVERB_PROPERTIES::ReverbDelay, 0, 0 },
	{ -2000000,  2000000, &REVERB_PROPERTIES::ReverbPan0, 0, 0 },
	{ -2000000,  2000000, &REVERB_PROPERTIES::ReverbPan1, 0, 0 },
	{ -2000000,  2000000, &REVERB_PROPERTIES::ReverbPan2, 0, 0 },
	{       75,      250, &REVERB_PROPERTIES::EchoTime, 0, 0 },
	{        0,     1000, &REVERB_PROPERTIES::EchoDepth, 0, 0 },
	{       40,     4000, &REVERB_PROPERTIES::ModulationTime, 0, 0 },
	{        0,     1000, &REVERB_PROPERTIES::ModulationDepth, 0, 0 },
	{  -100000,        0, &REVERB_PROPERTIES::AirAbsorptionHF, 0, 0 },
	{  1000000, 20000000, &REVERB_PROPERTIES::HFReference, 0, 0 },
	{    20000,  1000000, &REVERB_PROPERTIES::LFReference, 0, 0 },
	{        0,    10000, &REVERB_PROPERTIES::RoomRolloffFactor, 0, 0 },
	{        0,   100000, &REVERB_PROPERTIES::Diffusion, 0, 0 },
	{        0,   100000, &REVERB_PROPERTIES::Density, 0, 0 },
	{ 0, 0, 0, 0, 1 },
	{ 0, 0, 0, 0, 2 },
	{ 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 5 },
	{ 0, 0, 0, 0, 3 },
	{ 0, 0, 0, 0, 4 },
	{ 0, 0, 0, 0, 6 },
	{ 0, 0, 0, 0, 7 }
};
#define NUM_EAX_FIELDS (int(countof(EAXFields)))

static const char *EAXFieldNames[NUM_EAX_FIELDS+2] =
{
	"Environment",
	"EnvironmentSize",
	"EnvironmentDiffusion",
	"Room",
	"RoomHF",
	"RoomLF",
	"DecayTime",
	"DecayHFRatio",
	"DecayLFRatio",
	"Reflections",
	"ReflectionsDelay",
	"ReflectionsPanX",
	"ReflectionsPanY",
	"ReflectionsPanZ",
	"Reverb",
	"ReverbDelay",
	"ReverbPanX",
	"ReverbPanY",
	"ReverbPanZ",
	"EchoTime",
	"EchoDepth",
	"ModulationTime",
	"ModulationDepth",
	"AirAbsorptionHF",
	"HFReference",
	"LFReference",
	"RoomRolloffFactor",
	"Diffusion",
	"Density",
	"bReflectionsScale",
	"bReflectionsDelayScale",
	"bDecayTimeScale",
	"bDecayHFLimit",
	"bReverbScale",
	"bReverbDelayScale",
	"bEchoTimeScale",
	"bModulationTimeScale",
	"}",
	NULL
};

static const char *BoolNames[3] = { "False", "True", NULL };

static ReverbContainer Psychotic =
{
	NULL,
	"Psychotic",
	0x1900,
	true,
	false,
	{25,	1.0f,	0.50f, -1000,  -151,   0,   7.56f,  0.91f, 1.0f,  -626,  0.020f, 0.0f,0.0f,0.0f,   774, 0.030f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 4.00f, 1.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x1f }
};

static ReverbContainer Dizzy =
{
	&Psychotic,
	"Dizzy",
	0x1800,
	true,
	false,
	{24,	1.8f,	0.60f, -1000,  -400,   0,   17.23f, 0.56f, 1.0f,  -1713, 0.020f, 0.0f,0.0f,0.0f,  -613, 0.030f, 0.0f,0.0f,0.0f, 0.250f, 1.00f, 0.81f, 0.310f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x1f }
};

static ReverbContainer Drugged =
{
	&Dizzy,
	"Drugged",
	0x1700,
	true,
	false,
	{23,	1.9f,	0.50f, -1000,  0,      0,   8.39f,  1.39f, 1.0f,  -115,  0.002f, 0.0f,0.0f,0.0f,   985, 0.030f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 1.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x1f }
};

static ReverbContainer Underwater =
{
	&Drugged,
	"Underwater",
	0x1600,
	true,
	false,
	{22,	1.8f,	1.00f, -1000,  -4000,  0,   1.49f,  0.10f, 1.0f,   -449, 0.007f, 0.0f,0.0f,0.0f,  1700, 0.011f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 1.18f, 0.348f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer SewerPipe =
{
	&Underwater,
	"Sewer Pipe",
	0x1500,
	true,
	false,
	{21,	1.7f,	0.80f, -1000,  -1000,  0,   2.81f,  0.14f, 1.0f,    429, 0.014f, 0.0f,0.0f,0.0f,  1023, 0.021f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f,  80.0f,  60.0f, 0x3f }
};

static ReverbContainer ParkingLot =
{
	&SewerPipe,
	"Parking Lot",
	0x1400,
	true,
	false,
	{20,	8.3f,	1.00f, -1000,  0,      0,   1.65f,  1.50f, 1.0f,  -1363, 0.008f, 0.0f,0.0f,0.0f, -1153, 0.012f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x1f }
};

static ReverbContainer Plain =
{
	&ParkingLot,
	"Plain",
	0x1300,
	true,
	false,
	{19,	42.5f,	0.21f, -1000,  -2000,  0,   1.49f,  0.50f, 1.0f,  -2466, 0.179f, 0.0f,0.0f,0.0f, -1926, 0.100f, 0.0f,0.0f,0.0f, 0.250f, 1.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f,  21.0f, 100.0f, 0x3f }
};

static ReverbContainer Quarry =
{
	&Plain,
	"Quarry",
	0x1200,
	true,
	false,
	{18,	17.5f,	1.00f, -1000,  -1000,  0,   1.49f,  0.83f, 1.0f, -10000, 0.061f, 0.0f,0.0f,0.0f,   500, 0.025f, 0.0f,0.0f,0.0f, 0.125f, 0.70f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer Mountains =
{
	&Quarry,
	"Mountains",
	0x1100,
	true,
	false,
	{17,	100.0f, 0.27f, -1000,  -2500,  0,   1.49f,  0.21f, 1.0f,  -2780, 0.300f, 0.0f,0.0f,0.0f, -1434, 0.100f, 0.0f,0.0f,0.0f, 0.250f, 1.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f,  27.0f, 100.0f, 0x1f }
};

static ReverbContainer City =
{
	&Mountains,
	"City",
	0x1000,
	true,
	false,
	{16,	7.5f,	0.50f, -1000,  -800,   0,   1.49f,  0.67f, 1.0f,  -2273, 0.007f, 0.0f,0.0f,0.0f, -1691, 0.011f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f,  50.0f, 100.0f, 0x3f }
};

static ReverbContainer Forest =
{
	&City,
	"Forest",
	0x0F00,
	true,
	false,
	{15,	38.0f,	0.30f, -1000,  -3300,  0,   1.49f,  0.54f, 1.0f,  -2560, 0.162f, 0.0f,0.0f,0.0f,  -229, 0.088f, 0.0f,0.0f,0.0f, 0.125f, 1.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f,  79.0f, 100.0f, 0x3f }
};

static ReverbContainer Alley =
{
	&Forest,
	"Alley",
	0x0E00,
	true,
	false,
	{14,	7.5f,	0.30f, -1000,  -270,   0,   1.49f,  0.86f, 1.0f,  -1204, 0.007f, 0.0f,0.0f,0.0f,    -4, 0.011f, 0.0f,0.0f,0.0f, 0.125f, 0.95f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer StoneCorridor =
{
	&Alley,
	"Stone Corridor",
	0x0D00,
	true,
	false,
	{13,	13.5f,	1.00f, -1000,  -237,   0,   2.70f,  0.79f, 1.0f,  -1214, 0.013f, 0.0f,0.0f,0.0f,   395, 0.020f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer Hallway =
{
	&StoneCorridor,
	"Hallway",
	0x0C00,
	true,
	false,
	{12,	1.8f,	1.00f, -1000,  -300,   0,   1.49f,  0.59f, 1.0f,  -1219, 0.007f, 0.0f,0.0f,0.0f,   441, 0.011f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer CarpettedHallway =
{
	&Hallway,
	"Carpetted Hallway",
	0x0B00,
	true,
	false,
	{11,	1.9f,	1.00f, -1000,  -4000,  0,   0.30f,  0.10f, 1.0f,  -1831, 0.002f, 0.0f,0.0f,0.0f, -1630, 0.030f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer Hangar =
{
	&CarpettedHallway,
	"Hangar",
	0x0A00,
	true,
	false,
	{10,	50.3f,	1.00f, -1000,  -1000,  0,   10.05f, 0.23f, 1.0f,   -602, 0.020f, 0.0f,0.0f,0.0f,   198, 0.030f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer Arena =
{
	&Hangar,
	"Arena",
	0x0900,
	true,
	false,
	{9,	36.2f,	1.00f, -1000,  -698,   0,   7.24f,  0.33f, 1.0f,  -1166, 0.020f, 0.0f,0.0f,0.0f,    16, 0.030f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer Cave =
{
	&Arena,
	"Cave",
	0x0800,
	true,
	false,
	{8,	14.6f,	1.00f, -1000,  0,      0,   2.91f,  1.30f, 1.0f,   -602, 0.015f, 0.0f,0.0f,0.0f,  -302, 0.022f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x1f }
};

static ReverbContainer ConcertHall =
{
	&Cave,
	"Concert Hall",
	0x0700,
	true,
	false,
	{7,	19.6f,	1.00f, -1000,  -500,   0,   3.92f,  0.70f, 1.0f,  -1230, 0.020f, 0.0f,0.0f,0.0f,    -2, 0.029f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer Auditorium =
{
	&ConcertHall,
	"Auditorium",
	0x0600,
	true,
	false,
	{6,	21.6f,	1.00f, -1000,  -476,   0,   4.32f,  0.59f, 1.0f,   -789, 0.020f, 0.0f,0.0f,0.0f,  -289, 0.030f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer StoneRoom =
{
	&Auditorium,
	"Stone Room",
	0x0500,
	true,
	false,
	{5,	11.6f,	1.00f, -1000,  -300,   0,   2.31f,  0.64f, 1.0f,   -711, 0.012f, 0.0f,0.0f,0.0f,    83, 0.017f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer LivingRoom =
{
	&StoneRoom,
	"Living Room",
	0x0400,
	true,
	false,
	{4,	2.5f,	1.00f, -1000,  -6000,  0,   0.50f,  0.10f, 1.0f,  -1376, 0.003f, 0.0f,0.0f,0.0f, -1104, 0.004f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer Bathroom =
{
	&LivingRoom,
	"Bathroom",
	0x0300,
	true,
	false,
	{3,	1.4f,	1.00f, -1000,  -1200,  0,   1.49f,  0.54f, 1.0f,   -370, 0.007f, 0.0f,0.0f,0.0f,  1030, 0.011f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f,  60.0f, 0x3f }
};

static ReverbContainer Room =
{
	&Bathroom,
	"Room",
	0x0200,
	true,
	false,
	{2,	1.9f,	1.00f, -1000,  -454,   0,   0.40f,  0.83f, 1.0f,  -1646, 0.002f, 0.0f,0.0f,0.0f,    53, 0.003f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer PaddedCell =
{
	&Room,
	"Padded Cell",
	0x0100,
	true,
	false,
	{1,	1.4f,	1.00f, -1000,  -6000,  0,   0.17f,  0.10f, 1.0f,  -1204, 0.001f, 0.0f,0.0f,0.0f,   207, 0.002f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer Generic =
{
	&PaddedCell,
	"Generic",
	0x0001,
	true,
	false,
	{0,	7.5f,	1.00f, -1000,  -100,   0,   1.49f,  0.83f, 1.0f,  -2602, 0.007f, 0.0f,0.0f,0.0f,   200, 0.011f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f }
};

static ReverbContainer Off =
{
	&Generic,
	"Off",
	0x0000,
	true,
	false,
	{0,	7.5f,	1.00f, -10000, -10000, 0,   1.00f,  1.00f, 1.0f,  -2602, 0.007f, 0.0f,0.0f,0.0f,   200, 0.011f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f,   0.0f,   0.0f, 0x33f }
};

ReverbContainer *DefaultEnvironments[26] =
{
	&Off, &PaddedCell, &Room, &Bathroom, &LivingRoom, &StoneRoom, &Auditorium,
	&ConcertHall, &Cave, &Arena, &Hangar, &CarpettedHallway, &Hallway, &StoneCorridor,
	&Alley, &Forest, &City, &Mountains, &Quarry, &Plain, &ParkingLot, &SewerPipe,
	&Underwater, &Drugged, &Dizzy, &Psychotic
};

ReverbContainer *Environments = &Off;

ReverbContainer *S_FindEnvironment (const char *name)
{
	ReverbContainer *probe = Environments;

	if (name == NULL)
		return NULL;

	while (probe != NULL)
	{
		if (stricmp (probe->Name, name) == 0)
		{
			return probe;
		}
		probe = probe->Next;
	}
	return NULL;
}

ReverbContainer *S_FindEnvironment (int id)
{
	ReverbContainer *probe = Environments;

	while (probe != NULL && probe->ID < id)
	{
		probe = probe->Next;
	}
	return (probe && probe->ID == id ? probe : NULL);
}

void S_AddEnvironment (ReverbContainer *settings)
{
	ReverbContainer *probe = Environments;
	ReverbContainer **ptr = &Environments;

	while (probe != NULL && probe->ID < settings->ID)
	{
		ptr = &probe->Next;
		probe = probe->Next;
	}

	if (probe != NULL && probe->ID == settings->ID)
	{
		// Built-in environments cannot be changed
		if (!probe->Builtin)
		{
			settings->Next = probe->Next;
			*ptr = settings;
			delete[] const_cast<char *>(probe->Name);
			delete probe;
		}
	}
	else
	{
		settings->Next = probe;
		*ptr = settings;
	}
}

FArchive &operator<< (FArchive &arc, ReverbContainer *&env)
{
	WORD id;

	if (arc.IsStoring())
	{
		if (env != NULL)
		{
			arc << env->ID;
		}
		else
		{
			id = 0;
			arc << id;
		}
	}
	else
	{
		arc << id;
		env = S_FindEnvironment (id);
	}
	return arc;
}

static void ReadEAX ()
{
	const ReverbContainer *def;
	ReverbContainer *newenv;
	REVERB_PROPERTIES props;
	char *name;
	int id1, id2, i, j;
	bool inited[NUM_EAX_FIELDS];
	BYTE bools[32];

	while (SC_GetString ())
	{
		name = copystring (sc_String);
		SC_MustGetNumber ();
		id1 = sc_Number;
		SC_MustGetNumber ();
		id2 = sc_Number;
		SC_MustGetStringName ("{");
		memset (inited, 0, sizeof(inited));
		props.Flags = 0;
		while (SC_MustGetString (), NUM_EAX_FIELDS > (i = SC_MustMatchString (EAXFieldNames)))
		{
			if (EAXFields[i].Float)
			{
				SC_MustGetFloat ();
				props.*EAXFields[i].Float = clamp (sc_Float,
					float(EAXFields[i].Min)/1000.f,
					float(EAXFields[i].Max)/1000.f);
			}
			else if (EAXFields[i].Int)
			{
				SC_MustGetNumber ();
				props.*EAXFields[i].Int = (j = clamp (sc_Number,
					EAXFields[i].Min, EAXFields[i].Max));
				if (i == 0 && j != sc_Number)
				{
					SC_ScriptError ("The Environment field is out of range.");
				}
			}
			else
			{
				SC_MustGetString ();
				bools[EAXFields[i].Flag] = SC_MustMatchString (BoolNames);
			}
			inited[i] = true;
		}
		if (!inited[0])
		{
			SC_ScriptError ("Sound %s is missing an Environment field.", name);
		}

		// Add the new environment to the list, filling in uninitialized fields
		// with values from the standard environment specified.
		def = DefaultEnvironments[props.Environment];
		for (i = 0; i < NUM_EAX_FIELDS; ++i)
		{
			if (EAXFields[i].Float)
			{
				if (!inited[i])
				{
					props.*EAXFields[i].Float = def->Properties.*EAXFields[i].Float;
				}
			}
			else if (EAXFields[i].Int)
			{
				if (!inited[i])
				{
					props.*EAXFields[i].Int = def->Properties.*EAXFields[i].Int;
				}
			}
			else
			{
				if (!inited[i])
				{
					int mask = 1 << EAXFields[i].Flag;
					if (def->Properties.Flags & mask)
					{
						props.Flags |= mask;
					}
				}
				else
				{
					if (bools[EAXFields[i].Flag])
					{
						props.Flags |= 1 << EAXFields[i].Flag;
					}
				}
			}
		}

		newenv = new ReverbContainer;
		newenv->Next = NULL;
		newenv->Name = name;
		newenv->ID = (id1 << 8) | id2;
		newenv->Builtin = false;
		newenv->Properties = props;
		S_AddEnvironment (newenv);
	}
}

void S_ParseSndEax ()
{
	int lump, lastlump = 0;

	atterm (S_UnloadSndEax);

	while ((lump = Wads.FindLump ("SNDEAX", &lastlump)) != -1)
	{
		SC_OpenLumpNum (lump, "SNDEAX");
		ReadEAX ();
		SC_Close ();
	}
}

void S_UnloadSndEax ()
{
	ReverbContainer *probe = Environments;

	while (probe != NULL)
	{
		ReverbContainer *next = probe->Next;
		if (!probe->Builtin)
		{
			delete[] const_cast<char *>(probe->Name);
			delete probe;
		}
		probe = next;
	}
	Environments = &Off;
}
