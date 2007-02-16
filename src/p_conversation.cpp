#include <assert.h>

#include "actor.h"
#include "r_data.h"
#include "p_conversation.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "s_sound.h"
#include "m_menu.h"
#include "v_text.h"
#include "v_video.h"
#include "m_random.h"
#include "gi.h"
#include "templates.h"
#include "a_strifeglobal.h"
#include "a_keys.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "sound/i_music.h"

// The conversations as they exist inside a SCRIPTxx lump.
struct Response
{
	SDWORD GiveType;
	SDWORD Item[3];
	SDWORD Count[3];
	char Reply[32];
	char Yes[80];
	SDWORD Link;
	DWORD Log;
	char No[80];
};

struct Speech
{
	DWORD SpeakerType;
	SDWORD DropType;
	SDWORD ItemCheck[3];
	SDWORD Link;
	char Name[16];
	char Sound[8];
	char Backdrop[8];
	char Dialogue[320];
	Response Responses[5];
};

// The Teaser version of the game uses an older version of the structure
struct TeaserSpeech
{
	DWORD SpeakerType;
	SDWORD DropType;
	DWORD VoiceNumber;
	char Name[16];
	char Dialogue[320];
	Response Responses[5];
};

static FRandom pr_randomspeech("RandomSpeech");

void GiveSpawner (player_t *player, const PClass *type);

TArray<FStrifeDialogueNode *> StrifeDialogues;

// There were 344 types in Strife, and Strife conversations refer
// to their index in the mobjinfo table. This table indexes all
// the Strife actor types in the order Strife had them and is
// initialized as part of the actor's setup in infodefaults.cpp.
const PClass *StrifeTypes[1001];

static menu_t ConversationMenu;
static TArray<menuitem_t> ConversationItems;
static int ConversationPauseTic;
static bool ShowGold;

static void LoadScriptFile (const char *name);
static FStrifeDialogueNode *ReadRetailNode (FWadLump *lump, DWORD &prevSpeakerType);
static FStrifeDialogueNode *ReadTeaserNode (FWadLump *lump, DWORD &prevSpeakerType);
static void ParseReplies (FStrifeDialogueReply **replyptr, Response *responses);
static void DrawConversationMenu ();
static void PickConversationReply ();
static void CleanupConversationMenu ();
static void ConversationMenuEscaped ();

static FStrifeDialogueNode *CurNode, *PrevNode;
static FBrokenLines *DialogueLines;
static AActor *ConversationNPC, *ConversationPC;
static angle_t ConversationNPCAngle;
static bool ConversationFaceTalker;

#define NUM_RANDOM_LINES 10
#define NUM_RANDOM_GOODBYES 3

//============================================================================
//
// GetStrifeType
//
// Given an item type number, returns the corresponding PClass.
//
//============================================================================

static const PClass *GetStrifeType (int typenum)
{
	if (typenum > 0 && typenum < 1001)
	{
		return StrifeTypes[typenum];
	}
	return NULL;
}

//============================================================================
//
// P_LoadStrifeConversations
//
// Loads the SCRIPT00 and SCRIPTxx files for a corresponding map.
//
//============================================================================

void P_LoadStrifeConversations (const char *mapname)
{
	if (strnicmp (mapname, "MAP", 3) != 0)
	{
		return;
	}
	char scriptname[9] = { 'S','C','R','I','P','T',mapname[3],mapname[4],0 };

	LoadScriptFile ("SCRIPT00");
	LoadScriptFile (scriptname);
}

//============================================================================
//
// P_FreeStrifeConversations
//
//============================================================================

void P_FreeStrifeConversations ()
{
	FStrifeDialogueNode *node;

	while (StrifeDialogues.Pop (node))
	{
		delete node;
	}

	for (int i = 0; i < 344; ++i)
	{
		if (StrifeTypes[i] != NULL)
		{
			AActor * ac = GetDefaultByType (StrifeTypes[i]);
			if (ac != NULL) ac->Conversation = NULL;
		}
	}

	CurNode = NULL;
	PrevNode = NULL;
}

//============================================================================
//
// ncopystring
//
// If the string has no content, returns NULL. Otherwise, returns a copy.
//
//============================================================================

static char *ncopystring (const char *string)
{
	if (string == NULL || string[0] == 0)
	{
		return NULL;
	}
	return copystring (string);
}

//============================================================================
//
// LoadScriptFile
//
// Loads a SCRIPTxx file and converts it into a more useful internal format.
//
//============================================================================

static void LoadScriptFile (const char *name)
{
	int lumpnum = Wads.CheckNumForName (name);
	int numnodes, i;
	DWORD prevSpeakerType;
	FStrifeDialogueNode *node;
	FWadLump *lump;

	if (lumpnum < 0)
	{
		return;
	}

	numnodes = Wads.LumpLength (lumpnum);
	if (!(gameinfo.flags & GI_SHAREWARE))
	{
		// Strife scripts are always a multiple of 1516 bytes because each entry
		// is exactly 1516 bytes long.
		if (numnodes % 1516 != 0)
		{
			return;
		}
		numnodes /= 1516;
	}
	else
	{
		// And the teaser version has 1488-byte entries.
		if (numnodes % 1488 != 0)
		{
			return;
		}
		numnodes /= 1488;
	}

	lump = Wads.ReopenLumpNum (lumpnum);
	prevSpeakerType = 0;

	for (i = 0; i < numnodes; ++i)
	{
		if (!(gameinfo.flags & GI_SHAREWARE))
		{
			node = ReadRetailNode (lump, prevSpeakerType);
		}
		else
		{
			node = ReadTeaserNode (lump, prevSpeakerType);
		}
		StrifeDialogues.Push (node);
	}
	delete lump;
}

//============================================================================
//
// ReadRetailNode
//
// Converts a single dialogue node from the Retail version of Strife.
//
//============================================================================

static FStrifeDialogueNode *ReadRetailNode (FWadLump *lump, DWORD &prevSpeakerType)
{
	FStrifeDialogueNode *node;
	Speech speech;
	char fullsound[16];
	const PClass *type;
	int j;

	node = new FStrifeDialogueNode;

	lump->Read (&speech, sizeof(speech));

	// Byte swap all the ints in the original data
	speech.SpeakerType = LittleLong(speech.SpeakerType);
	speech.DropType = LittleLong(speech.DropType);
	speech.Link = LittleLong(speech.Link);

	// Assign the first instance of a conversation as the default for its
	// actor, so newly spawned actors will use this conversation by default.
	type = GetStrifeType (speech.SpeakerType);
	node->SpeakerType = type;
	if (prevSpeakerType != speech.SpeakerType)
	{
		if (type != NULL)
		{
			GetDefaultByType (type)->Conversation = node;
		}
		prevSpeakerType = speech.SpeakerType;
	}

	// Convert the rest of the data to our own internal format.
	node->Dialogue = ncopystring (speech.Dialogue);

	// The speaker's portrait, if any.
	speech.Backdrop[8] = 0;
	node->Backdrop = TexMan.AddPatch (speech.Backdrop);

	// The speaker's voice for this node, if any.
	speech.Sound[8] = 0;
	sprintf (fullsound, "svox/%s", speech.Sound);
	node->SpeakerVoice = S_FindSound (fullsound);

	// The speaker's name, if any.
	speech.Name[16] = 0;
	node->SpeakerName = ncopystring (speech.Name);

	// The item the speaker should drop when killed.
	node->DropType = GetStrifeType (speech.DropType);

	// Items you need to have to make the speaker use a different node.
	for (j = 0; j < 3; ++j)
	{
		node->ItemCheck[j] = GetStrifeType (speech.ItemCheck[j]);
	}
	node->ItemCheckNode = speech.Link;
	node->Children = NULL;

	ParseReplies (&node->Children, &speech.Responses[0]);

	return node;
}

//============================================================================
//
// ReadTeaserNode
//
// Converts a single dialogue node from the Teaser version of Strife.
//
//============================================================================

static FStrifeDialogueNode *ReadTeaserNode (FWadLump *lump, DWORD &prevSpeakerType)
{
	FStrifeDialogueNode *node;
	TeaserSpeech speech;
	char fullsound[16];
	const PClass *type;
	int j;

	node = new FStrifeDialogueNode;

	lump->Read (&speech, sizeof(speech));

	// Byte swap all the ints in the original data
	speech.SpeakerType = LittleLong(speech.SpeakerType);
	speech.DropType = LittleLong(speech.DropType);

	// Assign the first instance of a conversation as the default for its
	// actor, so newly spawned actors will use this conversation by default.
	type = GetStrifeType (speech.SpeakerType);
	node->SpeakerType = type;
	if (prevSpeakerType != speech.SpeakerType)
	{
		if (type != NULL)
		{
			GetDefaultByType (type)->Conversation = node;
		}
		prevSpeakerType = speech.SpeakerType;
	}

	// Convert the rest of the data to our own internal format.
	node->Dialogue = ncopystring (speech.Dialogue);

	// The Teaser version doesn't have portraits.
	node->Backdrop = -1;

	// The speaker's voice for this node, if any.
	if (speech.VoiceNumber != 0)
	{
		sprintf (fullsound, "svox/voc%u", speech.VoiceNumber);
		node->SpeakerVoice = S_FindSound (fullsound);
	}
	else
	{
		node->SpeakerVoice = 0;
	}

	// The speaker's name, if any.
	speech.Name[16] = 0;
	node->SpeakerName = ncopystring (speech.Name);

	// The item the speaker should drop when killed.
	node->DropType = GetStrifeType (speech.DropType);

	// Items you need to have to make the speaker use a different node.
	for (j = 0; j < 3; ++j)
	{
		node->ItemCheck[j] = NULL;
	}
	node->ItemCheckNode = 0;
	node->Children = NULL;

	ParseReplies (&node->Children, &speech.Responses[0]);

	return node;
}

//============================================================================
//
// ParseReplies
//
// Convert PC responses. Rather than being stored inside the main node, they
// hang off it as a singly-linked list, so no space is wasted on replies that
// don't even matter.
//
//============================================================================

static void ParseReplies (FStrifeDialogueReply **replyptr, Response *responses)
{
	FStrifeDialogueReply *reply;
	int j, k;

	// Byte swap first.
	for (j = 0; j < 5; ++j)
	{
		responses[j].GiveType = LittleLong(responses[j].GiveType);
		responses[j].Link = LittleLong(responses[j].Link);
		responses[j].Log = LittleLong(responses[j].Log);
		for (k = 0; k < 3; ++k)
		{
			responses[j].Item[k] = LittleLong(responses[j].Item[k]);
			responses[j].Count[k] = LittleLong(responses[j].Count[k]);
		}
	}

	for (j = 0; j < 5; ++j)
	{
		Response *rsp = &responses[j];

		// If the reply has no text and goes nowhere, then it doesn't
		// need to be remembered.
		if (rsp->Reply[0] == 0 && rsp->Link == 0)
		{
			continue;
		}
		reply = new FStrifeDialogueReply;

		// The next node to use when this reply is chosen.
		reply->NextNode = rsp->Link;

		// The message to record in the log for this reply.
		reply->LogNumber = rsp->Log;

		// The item to receive when this reply is used.
		reply->GiveType = GetStrifeType (rsp->GiveType);

		// Do you need anything special for this reply to succeed?
		for (k = 0; k < 3; ++k)
		{
			reply->ItemCheck[k] = GetStrifeType (rsp->Item[k]);
			reply->ItemCheckAmount[k] = rsp->Count[k];
		}

		// ReplyLines is calculated when the menu is shown. It is just Reply
		// with word wrap turned on.
		reply->ReplyLines = NULL;

		// If the first item check has a positive amount required, then
		// add that to the reply string. Otherwise, use the reply as-is.
		if (rsp->Count[0] > 0)
		{
			char moneystr[128];

			sprintf (moneystr, "%s for %u", rsp->Reply, rsp->Count[0]);
			reply->Reply = copystring (moneystr);
			reply->NeedsGold = true;
		}
		else
		{
			reply->Reply = copystring (rsp->Reply);
			reply->NeedsGold = false;
		}

		// QuickYes messages are shown when you meet the item checks.
		// QuickNo messages are shown when you don't.
		if (rsp->Yes[0] == '_' && rsp->Yes[1] == 0)
		{
			reply->QuickYes = NULL;
		}
		else
		{
			reply->QuickYes = ncopystring (rsp->Yes);
		}
		if (reply->ItemCheck[0] != 0)
		{
			reply->QuickNo = ncopystring (rsp->No);
		}
		else
		{
			reply->QuickNo = NULL;
		}
		reply->Next = *replyptr;
		*replyptr = reply;
		replyptr = &reply->Next;
	}
}

//============================================================================
//
// FStrifeDialogueNode :: ~FStrifeDialogueNode
//
//============================================================================

FStrifeDialogueNode::~FStrifeDialogueNode ()
{
	if (SpeakerName != NULL) delete[] SpeakerName;
	if (Dialogue != NULL) delete[] Dialogue;
	FStrifeDialogueReply *tokill = Children;
	while (tokill != NULL)
	{
		FStrifeDialogueReply *next = tokill->Next;
		delete tokill;
		tokill = next;
	}
}

//============================================================================
//
// FStrifeDialogueReply :: ~FStrifeDialogueReply
//
//============================================================================

FStrifeDialogueReply::~FStrifeDialogueReply ()
{
	if (Reply != NULL) delete[] Reply;
	if (QuickYes != NULL) delete[] QuickYes;
	if (QuickNo != NULL) delete[] QuickNo;
	if (ReplyLines != NULL) V_FreeBrokenLines (ReplyLines);
}

//============================================================================
//
// FindNode
//
// Returns the index that matches the given conversation node.
//
//============================================================================

static int FindNode (const FStrifeDialogueNode *node)
{
	int rootnode = 0;

	while (StrifeDialogues[rootnode] != node)
	{
		rootnode++;
	}
	return rootnode;
}

//============================================================================
//
// CheckStrifeItem
//
// Checks if you have an item. A NULL itemtype is always considered to be
// present.
//
//============================================================================

static bool CheckStrifeItem (const PClass *itemtype, int amount=-1)
{
	AInventory *item;

	if (itemtype == NULL || amount == 0)
		return true;

	item = ConversationPC->FindInventory (itemtype);
	if (item == NULL)
		return false;

	return amount < 0 || item->Amount >= amount;
}

//============================================================================
//
// TakeStrifeItem
//
// Takes away some of an item, unless that item is special and should not
// be removed.
//
//============================================================================

static void TakeStrifeItem (const PClass *itemtype, int amount)
{
	if (itemtype == NULL || amount == 0)
		return;

	// Don't take quest items.
	if (itemtype->IsDescendantOf (PClass::FindClass(NAME_QuestItem)))
		return;

	// Don't take keys
	if (itemtype->IsDescendantOf (RUNTIME_CLASS(AKey)))
		return;

	// Don't take the sigil
	if (itemtype == RUNTIME_CLASS(ASigil))
		return;

	AInventory *item = ConversationPC->FindInventory (itemtype);
	if (item != NULL)
	{
		item->Amount -= amount;
		if (item->Amount <= 0)
		{
			item->Destroy ();
		}
	}
}

CUSTOM_CVAR(Float, dlg_musicvolume, 1.0f, CVAR_ARCHIVE)
{
	if (self < 0.f) self = 0.f;
	else if (self > 1.f) self = 1.f;
}

//============================================================================
//
// P_StartConversation
//
// Begins a conversation between a PC and NPC.
// FIXME: Make this work in multiplayer.
//
//============================================================================

void P_StartConversation (AActor *npc, AActor *pc, bool facetalker, bool saveangle)
{
	AActor *oldtarget;
	FStrifeDialogueReply *reply;
	menuitem_t item;
	const char *toSay;
	int i, j;

	pc->momx = pc->momy = 0;	// Stop moving
	pc->player->momx = pc->player->momy = 0;

	if (pc->player - players != consoleplayer)
		return;

	ConversationPC = pc;
	ConversationNPC = npc;

	CurNode = npc->Conversation;

	if (pc->player == &players[consoleplayer])
	{
		S_Sound (CHAN_VOICE, "misc/chat", 1, ATTN_NONE);
	}

	npc->reactiontime = 2;
	ConversationFaceTalker = facetalker;
	if (saveangle)
	{
		ConversationNPCAngle = npc->angle;
	}
	oldtarget = npc->target;
	npc->target = pc;
	if (facetalker)
	{
		A_FaceTarget (npc);
		pc->angle = R_PointToAngle2 (pc->x, pc->y, npc->x, npc->y);
	}
	if ((npc->flags & MF_FRIENDLY) || (npc->flags4 & MF4_NOHATEPLAYERS))
	{
		npc->target = oldtarget;
	}

	// Check if we should jump to another node
	while (CurNode->ItemCheck[0] != NULL)
	{
		if (CheckStrifeItem (CurNode->ItemCheck[0]) &&
			CheckStrifeItem (CurNode->ItemCheck[1]) &&
			CheckStrifeItem (CurNode->ItemCheck[2]))
		{
			int root = FindNode (ConversationNPC->GetDefault()->Conversation);
			CurNode = StrifeDialogues[root + CurNode->ItemCheckNode - 1];
		}
		else
		{
			break;
		}
	}

	if (CurNode->SpeakerVoice != 0)
	{
		I_SetMusicVolume(dlg_musicvolume);
		S_SoundID (npc, CHAN_VOICE|CHAN_NOPAUSE, CurNode->SpeakerVoice, 1, ATTN_NORM);
	}

	// Set up the menu
	ConversationMenu.PreDraw = DrawConversationMenu;
	ConversationMenu.EscapeHandler = ConversationMenuEscaped;

	// Format the speaker's message.
	toSay = CurNode->Dialogue;
	if (strncmp (toSay, "RANDOM_", 7) == 0)
	{
		FString dlgtext;

		dlgtext.Format("TXT_%s_%02d", toSay, 1+(pr_randomspeech() % NUM_RANDOM_LINES));
		toSay = GStrings[dlgtext.GetChars()];
		if (toSay==NULL) toSay = "Go away!";	// Ok, it's lame - but it doesn't look like an error to the player. ;)
	}
	DialogueLines = V_BreakLines (screen->Font, screen->GetWidth()/CleanXfac-24*2, toSay);

	// Fill out the possible choices
	ShowGold = false;
	item.type = numberedmore;
	item.e.mfunc = PickConversationReply;
	for (reply = CurNode->Children, i = 1; reply != NULL; reply = reply->Next)
	{
		if (reply->Reply == NULL)
		{
			continue;
		}
		ShowGold |= reply->NeedsGold;
		reply->ReplyLines = V_BreakLines (screen->Font, 320-50-10, reply->Reply);
		for (j = 0; reply->ReplyLines[j].Width >= 0; ++j)
		{
			item.label = reply->ReplyLines[j].Text.LockBuffer();
			item.b.position = j == 0 ? i : 0;
			item.c.extra = reply;
			ConversationItems.Push (item);
		}
		++i;
	}
	char goodbye[25];
	sprintf(goodbye, "TXT_RANDOMGOODBYE_%d", 1+(pr_randomspeech() % NUM_RANDOM_GOODBYES));
	item.label = (char*)GStrings[goodbye];
	if (item.label == NULL) item.label = "Bye.";
	item.b.position = i;
	item.c.extra = NULL;
	ConversationItems.Push (item);

	// Determine where the top of the reply list should be positioned.
	i = (gameinfo.gametype & GAME_Raven) ? 9 : 8;
	ConversationMenu.y = MIN<int> (140, 192 - ConversationItems.Size() * i);
	for (i = 0; DialogueLines[i].Width >= 0; ++i)
	{ }
	i = 44 + i * 10;
	if (ConversationMenu.y - 100 < i - screen->GetHeight() / CleanYfac / 2)
	{
		ConversationMenu.y = i - screen->GetHeight() / CleanYfac / 2 + 100;
	}
	ConversationMenu.indent = 50;

	// Finish setting up the menu
	ConversationMenu.items = &ConversationItems[0];
	ConversationMenu.numitems = ConversationItems.Size();
	if (CurNode != PrevNode)
	{ // Only reset the selection if showing a different menu.
		ConversationMenu.lastOn = 0;
		PrevNode = CurNode;
	}
	ConversationMenu.DontDim = true;

	// And open the menu
	M_StartControlPanel (false);
	OptionsActive = true;
	menuactive = MENU_OnNoPause;
	ConversationPauseTic = gametic + 20;
	M_SwitchMenu (&ConversationMenu);
}

//============================================================================
//
// P_ResumeConversation
//
// Resumes a conversation that was interrupted by a slideshow.
//
//============================================================================

void P_ResumeConversation ()
{
	if (ConversationPC != NULL && ConversationNPC != NULL)
	{
		P_StartConversation (ConversationNPC, ConversationPC, ConversationFaceTalker, false);
	}
}

//============================================================================
//
// DrawConversationMenu
//
//============================================================================

static void DrawConversationMenu ()
{
	const char *speakerName;
	int i, x, y, linesize;

	assert (DialogueLines != NULL);
	assert (CurNode != NULL);

	if (CurNode == NULL)
	{
		M_ClearMenus ();
		return;
	}

	if (ConversationPauseTic < gametic)
	{
		menuactive = MENU_On;
	}

	if (CurNode->Backdrop >= 0)
	{
		screen->DrawTexture (TexMan(CurNode->Backdrop), 0, 0, DTA_320x200, true, TAG_DONE);
	}
	x = 16 * screen->GetWidth() / 320;
	y = 16 * screen->GetHeight() / 200;
	linesize = 10 * CleanYfac;

	// Who is talking to you?
	if (CurNode->SpeakerName != NULL)
	{
		speakerName = CurNode->SpeakerName;
	}
	else
	{
		speakerName = ConversationNPC->GetClass()->Meta.GetMetaString (AMETA_StrifeName);
		if (speakerName == NULL)
		{
			speakerName = "Person";
		}
	}

	// Dim the screen behind the dialogue (but only if there is no backdrop).
	if (CurNode->Backdrop <= 0)
	{
		for (i = 0; DialogueLines[i].Width >= 0; ++i)
		{ }
		screen->Dim (0, 0.45f, 14 * screen->GetWidth() / 320, 13 * screen->GetHeight() / 200,
			308 * screen->GetWidth() / 320 - 14 * screen->GetWidth () / 320,
			speakerName == NULL ? linesize * i + 6 * CleanYfac
			: linesize * i + 6 * CleanYfac + linesize * 3/2);
	}

	// Dim the screen behind the PC's choices.
	screen->Dim (0, 0.45f, (24-160) * CleanXfac + screen->GetWidth()/2,
		(ConversationMenu.y - 2 - 100) * CleanYfac + screen->GetHeight()/2,
		272 * CleanXfac,
		MIN(ConversationMenu.numitems * (gameinfo.gametype & GAME_Raven ? 9 : 8) + 4,
		200 - ConversationMenu.y) * CleanYfac);

	if (speakerName != NULL)
	{
		screen->DrawText (CR_WHITE, x, y, speakerName,
			DTA_CleanNoMove, true, TAG_DONE);
		y += linesize * 3 / 2;
	}
	x = 24 * screen->GetWidth() / 320;
	for (i = 0; DialogueLines[i].Width >= 0; ++i)
	{
		screen->DrawText (CR_UNTRANSLATED, x, y, DialogueLines[i].Text,
			DTA_CleanNoMove, true, TAG_DONE);
		y += linesize;
	}

	if (ShowGold)
	{
		AInventory *coin = ConversationPC->FindInventory (RUNTIME_CLASS(ACoin));
		char goldstr[32];

		sprintf (goldstr, "%d", coin != NULL ? coin->Amount : 0);
		screen->DrawText (CR_GRAY, 21, 191, goldstr, DTA_320x200, true,
			DTA_FillColor, 0, DTA_Alpha, HR_SHADOW, TAG_DONE);
		screen->DrawTexture (TexMan(((AInventory *)GetDefaultByType (RUNTIME_CLASS(ACoin)))->Icon),
			3, 190, DTA_320x200, true,
			DTA_FillColor, 0, DTA_Alpha, HR_SHADOW, TAG_DONE);
		screen->DrawText (CR_GRAY, 20, 190, goldstr, DTA_320x200, true, TAG_DONE);
		screen->DrawTexture (TexMan(((AInventory *)GetDefaultByType (RUNTIME_CLASS(ACoin)))->Icon),
			2, 189, DTA_320x200, true, TAG_DONE);
	}
}

//============================================================================
//
// PickConversationReply
//
// FIXME: Make this work in multiplayer
//
//============================================================================

static void PickConversationReply ()
{
	const char *replyText = NULL;
	FStrifeDialogueReply *reply = (FStrifeDialogueReply *)ConversationItems[ConversationMenu.lastOn].c.extra;
	bool takestuff;
	int i;

	M_ClearMenus ();
	CleanupConversationMenu ();
	if (reply == NULL)
	{
		ConversationNPC->angle = ConversationNPCAngle;
		return;
	}

	// Check if you have the requisite items for this choice
	for (i = 0; i < 3; ++i)
	{
		if (!CheckStrifeItem (reply->ItemCheck[i], reply->ItemCheckAmount[i]))
		{
			// No, you don't. Say so and let the NPC animate negatively.
			if (reply->QuickNo)
			{
				Printf ("%s\n", reply->QuickNo);
			}
			ConversationNPC->ConversationAnimation (2);
			ConversationNPC->angle = ConversationNPCAngle;
			return;
		}
	}

	// Yay, you do! Let the NPC animate affirmatively.
	ConversationNPC->ConversationAnimation (1);

	// If this reply gives you something, then try to receive it.
	takestuff = true;
	if (reply->GiveType != NULL)
	{
		if (reply->GiveType->IsDescendantOf(RUNTIME_CLASS(AInventory)))
		{
			if (reply->GiveType->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
			{
				if (players[consoleplayer].mo->FindInventory(reply->GiveType) != NULL)
				{
					takestuff = false;
				}
			}
	
			if (takestuff)
			{
				AInventory *item = static_cast<AInventory *> (Spawn (reply->GiveType, 0, 0, 0, NO_REPLACE));
				// Items given here should not count as items!
				if (item->flags & MF_COUNTITEM)
				{
					level.total_items--;
					item->flags &= ~MF_COUNTITEM;
				}
				if (item->IsA(RUNTIME_CLASS(AFlameThrower)))
				{
					// The flame thrower gives less ammo when given in a dialog
					static_cast<AWeapon*>(item)->AmmoGive1 = 40;
				}
				item->flags |= MF_DROPPED;
				if (!item->TryPickup (players[consoleplayer].mo))
				{
					item->Destroy ();
					takestuff = false;
				}
			}
		}
		else
		{
			// Trying to give a non-inventory item.
			takestuff = false;
			Printf("Attempting to give non-inventory item %s\n", reply->GiveType->TypeName.GetChars());
		}
	}

	// Take away required items if the give was successful or none was needed.
	if (takestuff)
	{
		for (i = 0; i < 3; ++i)
		{
			TakeStrifeItem (reply->ItemCheck[i], reply->ItemCheckAmount[i]);
		}
		replyText = reply->QuickYes;
	}
	else
	{
		replyText = "You seem to have enough!";
	}

	// Update the quest log, if needed.
	if (reply->LogNumber != 0)
	{
		players[consoleplayer].SetLogNumber (reply->LogNumber);
	}

	if (replyText != NULL)
	{
		Printf ("%s\n", replyText);
	}

	// Does this reply alter the speaker's conversation node? If NextNode is positive,
	// the next time they talk, the will show the new node. If it is negative, then they
	// will show the new node right away without terminating the dialogue.
	if (reply->NextNode != 0)
	{
		int rootnode = FindNode (ConversationNPC->GetDefault()->Conversation);
		if (reply->NextNode < 0)
		{
			ConversationNPC->Conversation = StrifeDialogues[rootnode - reply->NextNode - 1];
			if (gameaction != ga_slideshow)
			{
				P_StartConversation (ConversationNPC, players[consoleplayer].mo, ConversationFaceTalker, false);
				return;
			}
			else
			{
				S_StopSound (ConversationNPC, CHAN_VOICE);
			}
		}
		else
		{
			ConversationNPC->Conversation = StrifeDialogues[rootnode + reply->NextNode - 1];
		}
	}

	ConversationNPC->angle = ConversationNPCAngle;
	I_SetMusicVolume(1.f);
}

//============================================================================
//
// CleanupConversationMenu
//
// Release the resources used to create the most recent conversation menu.
//
//============================================================================

void CleanupConversationMenu ()
{
	FStrifeDialogueReply *reply;

	if (CurNode != NULL)
	{
		for (reply = CurNode->Children; reply != NULL; reply = reply->Next)
		{
			if (reply->ReplyLines != NULL)
			{
				V_FreeBrokenLines (reply->ReplyLines);
				reply->ReplyLines = NULL;
			}
		}
		CurNode = NULL;
	}
	if (DialogueLines != NULL)
	{
		V_FreeBrokenLines (DialogueLines);
		DialogueLines = NULL;
	}
	ConversationItems.Clear ();
	I_SetMusicVolume(1.f);
}

//============================================================================
//
// ConversationMenuEscaped
//
// Called when the user presses escape to leave tho conversation menu.
//
//============================================================================

void ConversationMenuEscaped ()
{
	CleanupConversationMenu ();
	ConversationNPC->angle = ConversationNPCAngle;
}
