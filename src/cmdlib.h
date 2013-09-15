// cmdlib.h

#ifndef __CMDLIB__
#define __CMDLIB__


#include "doomtype.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>

// the dec offsetof macro doesnt work very well...
#define myoffsetof(type,identifier) ((size_t)&((type *)1)->identifier - 1)

int		Q_filelength (FILE *f);
bool FileExists (const char *filename);
bool DirEntryExists (const char *pathname);

extern	FString progdir;

void	FixPathSeperator (char *path);
static void	inline FixPathSeperator (FString &path) { path.ReplaceChars('\\', '/'); }

void 	DefaultExtension (char *path, const char *extension);
void 	DefaultExtension (FString &path, const char *extension);
// [BC]
void 	ForceExtension( char *pszPath, const char *pszExtension );

FString	ExtractFilePath (const char *path);
FString	ExtractFileBase (const char *path, bool keep_extension=false);

int		ParseHex (const char *str);
int 	ParseNum (const char *str);
bool	IsNum (const char *str);		// [RH] added

char	*copystring(const char *s);
void	ReplaceString (char **ptr, const char *str);

bool CheckWildcards (const char *pattern, const char *text);

void FormatGUID (char *buffer, size_t buffsize, const GUID &guid);

const char *myasctime ();

int strbin (char *str);
FString strbin1 (const char *start);
void CleanseString (char *str);

void CreatePath(const char * fn);

FString ExpandEnvVars(const char *searchpathstring);
FString NicePath(const char *path);

#endif
