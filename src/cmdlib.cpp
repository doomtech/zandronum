// cmdlib.c (mostly borrowed from the Q2 source)

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif
#include "doomtype.h"
#include "cmdlib.h"
#include "i_system.h"
#include "v_text.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

/*
progdir will hold the path up to the game directory, including the slash

  f:\quake\
  /raid/quake/

gamedir will hold progdir + the game directory (id1, id2, etc)

  */

FString progdir;

static inline bool IsSeperator (int c)
{
	if (c == '/')
		return true;
#ifdef WIN32
	if (c == '\\' || c == ':')
		return true;
#endif
	return false;
}

void FixPathSeperator (char *path)
{
	while (*path)
	{
		if (*path == '\\')
			*path = '/';
		path++;
	}
}

char *copystring (const char *s)
{
	char *b;
	if (s)
	{
		size_t len = strlen (s) + 1;
		b = new char[len];
		memcpy (b, s, len);
	}
	else
	{
		b = new char[1];
		b[0] = '\0';
	}
	return b;
}


void ReplaceString (char **ptr, const char *str)
{
	if (*ptr)
	{
		if (*ptr == str)
			return;
		delete[] *ptr;
	}
	*ptr = copystring (str);
}

/*
=============================================================================

						MISC FUNCTIONS

=============================================================================
*/


/*
================
Q_filelength
================
*/
int Q_filelength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}


/*
==============
FileExists
==============
*/
bool FileExists (const char *filename)
{
	FILE *f;

	// [RH] Empty filenames are never there
	if (*filename == 0)
		return false;

	f = fopen (filename, "r");
	if (!f)
		return false;
	fclose (f);
	return true;
}

void DefaultExtension (char *path, const char *extension)
{
	char *src;
//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen(path) - 1;

	while (src != path && !IsSeperator(*src))
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strcat (path, extension);
}

void DefaultExtension (FString &path, const char *extension)
{
	const char *src = &path[int(path.Len())-1];

	while (src != &path[0] && !IsSeperator(*src))
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	path += extension;
}

//=============================================================================
//
//	[BC] ForceExtension
//
//	If the string doesn't have an extension, this adds the extension. If it does,
//	the existing extension is replaced with the given extension.
//
//=============================================================================
void ForceExtension( char *pszPath, const char *pszExtension )
{
	char	*psz;

	// Add the extension if it's the string doesn't already have one.
	DefaultExtension( pszPath, pszExtension );

	// We need at least 4 characters for the extension, and one for the null character.
	if ( strlen( pszPath ) < 5 )
		return;

	psz = pszPath + strlen( pszPath ) - 4;
	strncpy( psz, pszExtension, 4 );
}


/*
====================
Extract file parts
====================
*/
// FIXME: should include the slash, otherwise
// backing to an empty path will be wrong when appending a slash
FString ExtractFilePath (const char *path)
{
	const char *src;

	src = path + strlen(path) - 1;

//
// back up until a \ or the start
//
	while (src != path && !IsSeperator(*(src-1)))
		src--;

	return FString(path, src - path);
}

FString ExtractFileBase (const char *path, bool include_extension)
{
	const char *src, *dot;

	src = path + strlen(path) - 1;

	if (src >= path)
	{
		// back up until a / or the start
		while (src != path && !IsSeperator(*(src-1)))
			src--;

		// Check for files with drive specification but no path
#if defined(_WIN32) || defined(DOS)
		if (src == path && src[0] != 0)
		{
			if (src[1] == ':')
				src += 2;
		}
#endif

		if (!include_extension)
		{
			dot = src;
			while (*dot && *dot != '.')
			{
				dot++;
			}
			return FString(src, dot - src);
		}
		else
		{
			return FString(src);
		}
	}
	return FString();
}


/*
==============
ParseNum / ParseHex
==============
*/
int ParseHex (const char *hex)
{
	const char *str;
	int num;

	num = 0;
	str = hex;

	while (*str)
	{
		num <<= 4;
		if (*str >= '0' && *str <= '9')
			num += *str-'0';
		else if (*str >= 'a' && *str <= 'f')
			num += 10 + *str-'a';
		else if (*str >= 'A' && *str <= 'F')
			num += 10 + *str-'A';
		else {
			Printf ("Bad hex number: %s\n",hex);
			return 0;
		}
		str++;
	}

	return num;
}


int ParseNum (const char *str)
{
	if (str[0] == '$')
		return ParseHex (str+1);
	if (str[0] == '0' && str[1] == 'x')
		return ParseHex (str+2);
	return atol (str);
}


// [RH] Returns true if the specified string is a valid decimal number

bool IsNum (const char *str)
{
	while (*str)
	{
		if (((*str < '0') || (*str > '9')) && (*str != '-'))
		{
			return false;
		}
		str++;
	}
	return true;
}

// [RH] Checks if text matches the wildcard pattern using ? or *

bool CheckWildcards (const char *pattern, const char *text)
{
	if (pattern == NULL || text == NULL)
		return true;

	while (*pattern)
	{
		if (*pattern == '*')
		{
			char stop = tolower (*++pattern);
			while (*text && tolower(*text) != stop)
			{
				text++;
			}
			if (*text && tolower(*text) == stop)
			{
				if (CheckWildcards (pattern, text++))
				{
					return true;
				}
				pattern--;
			}
		}
		else if (*pattern == '?' || tolower(*pattern) == tolower(*text))
		{
			pattern++;
			text++;
		}
		else
		{
			return false;
		}
	}
	return (*pattern | *text) == 0;
}

// [RH] Print a GUID to a text buffer using the standard format.

void FormatGUID (char *buffer, size_t buffsize, const GUID &guid)
{
	mysnprintf (buffer, buffsize, "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
		(uint32)guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1],
		guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5],
		guid.Data4[6], guid.Data4[7]);
}

// [RH] Returns the current local time as ASCII, even if it's too early
const char *myasctime ()
{
	time_t clock;
	struct tm *lt;

	time (&clock);
	lt = localtime (&clock);
	if (lt != NULL)
	{
		return asctime (lt);
	}
	else
	{
		return "Pre Jan 01 00:00:00 1970\n";
	}
}

/************************************************************************/
/* CreatePath: creates a directory including all levels necessary    	*/
/*																		*/
/************************************************************************/
#ifdef _WIN32
void DoCreatePath(const char *fn)
{
	char drive[_MAX_DRIVE];
	char path[PATH_MAX];
	char p[PATH_MAX];
	int i;

	_splitpath(fn,drive,path,NULL,NULL);
	_makepath(p,drive,path,NULL,NULL);
	i=(int)strlen(p);
	if (p[i-1]=='/' || p[i-1]=='\\') p[i-1]=0;
	if (*path) DoCreatePath(p);
	_mkdir(p);
}

void CreatePath(const char *fn)
{
	char c = fn[strlen(fn)-1];

	if (c != '\\' && c != '/') 
	{
		FString name(fn);
		name += '/';
		DoCreatePath(name);
	}
	else
	{
		DoCreatePath(fn);
	}
}
#else
void CreatePath(const char *fn)
{
	char *copy, *p;
 
	if (fn[0] == '/' && fn[1] == '\0')
	{
		return;
	}
	p = copy = strdup(fn);
	do
	{
		p = strchr(p + 1, '/');
		if (p != NULL)
		{
			*p = '\0';
		}
		struct stat info;
		if (stat(copy, &info) == 0)
		{
			if (info.st_mode & S_IFDIR)
				goto exists;
		}
		if (mkdir(copy, 0755) == -1)
		{ // failed
			return;
		}
exists:	if (p != NULL)
		{
			*p = '/';
		}
	} while (p);
	free(copy);
}
#endif

// [RH] Replaces the escape sequences in a string with actual escaped characters.
// This operation is done in-place. The result is the new length of the string.

int strbin (char *str)
{
	char *start = str;
	char *p = str, c;
	int i;

	while ( (c = *p++) ) {
		if (c != '\\') {
			*str++ = c;
		} else {
			switch (*p) {
				case 'a':
					*str++ = '\a';
					break;
				case 'b':
					*str++ = '\b';
					break;
				case 'c':
					*str++ = '\034';	// TEXTCOLOR_ESCAPE
					break;
				case 'f':
					*str++ = '\f';
					break;
				case 'n':
					*str++ = '\n';
					break;
				case 't':
					*str++ = '\t';
					break;
				case 'r':
					*str++ = '\r';
					break;
				case 'v':
					*str++ = '\v';
					break;
				case '?':
					*str++ = '\?';
					break;
				case '\n':
					break;
				case 'x':
				case 'X':
					c = 0;
					p++;
					for (i = 0; i < 2; i++) {
						c <<= 4;
						if (*p >= '0' && *p <= '9')
							c += *p-'0';
						else if (*p >= 'a' && *p <= 'f')
							c += 10 + *p-'a';
						else if (*p >= 'A' && *p <= 'F')
							c += 10 + *p-'A';
						else
							break;
						p++;
					}
					*str++ = c;
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					c = 0;
					for (i = 0; i < 3; i++) {
						c <<= 3;
						if (*p >= '0' && *p <= '7')
							c += *p-'0';
						else
							break;
						p++;
					}
					*str++ = c;
					break;
				default:
					*str++ = *p;
					break;
			}
			p++;
		}
	}
	*str = 0;
	return str - start;
}

// [RH] Replaces the escape sequences in a string with actual escaped characters.
// This operation is done in-place. The result is the new length of the string.

FString strbin1 (const char *start)
{
	FString result;
	const char *p = start;
	char c;
	int i;

	while ( (c = *p++) ) {
		if (c != '\\') {
			result << c;
		} else {
			switch (*p) {
				case 'a':
					result << '\a';
					break;
				case 'b':
					result << '\b';
					break;
				case 'c':
					result << '\034';	// TEXTCOLOR_ESCAPE
					break;
				case 'f':
					result << '\f';
					break;
				case 'n':
					result << '\n';
					break;
				case 't':
					result << '\t';
					break;
				case 'r':
					result << '\r';
					break;
				case 'v':
					result << '\v';
					break;
				case '?':
					result << '\?';
					break;
				case '\n':
					break;
				case 'x':
				case 'X':
					c = 0;
					p++;
					for (i = 0; i < 2; i++) {
						c <<= 4;
						if (*p >= '0' && *p <= '9')
							c += *p-'0';
						else if (*p >= 'a' && *p <= 'f')
							c += 10 + *p-'a';
						else if (*p >= 'A' && *p <= 'F')
							c += 10 + *p-'A';
						else
							break;
						p++;
					}
					result << c;
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					c = 0;
					for (i = 0; i < 3; i++) {
						c <<= 3;
						if (*p >= '0' && *p <= '7')
							c += *p-'0';
						else
							break;
						p++;
					}
					result << c;
					break;
				default:
					result << *p;
					break;
			}
			p++;
		}
	}
	return result;
}

//==========================================================================
//
// CleanseString
//
// Does some mild sanity checking on a string: If it ends with an incomplete
// color escape, the escape is removed.
//
//==========================================================================

void CleanseString(char *str)
{
	char *escape = strrchr(str, TEXTCOLOR_ESCAPE);
	if (escape != NULL)
	{
		if (escape[1] == '\0')
		{
			*escape = '\0';
		}
		else if (escape[1] == '[')
		{
			char *close = strchr(escape + 2, ']');
			if (close == NULL)
			{
				*escape = '\0';
			}
		}
	}
}

//==========================================================================
//
// ExpandEnvVars
//
// Expands environment variable references in a string. Intended primarily
// for use with IWAD search paths in config files.
//
//==========================================================================

FString ExpandEnvVars(const char *searchpathstring)
{
	static const char envvarnamechars[] =
		"01234567890"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"_"
		"abcdefghijklmnopqrstuvwxyz";

	if (searchpathstring == NULL)
		return FString("");

	const char *dollar = strchr(searchpathstring, '$');
	if (dollar == NULL)
	{
		return FString(searchpathstring);
	}

	const char *nextchars = searchpathstring;
	FString out = FString(searchpathstring, dollar - searchpathstring);
	while ( (dollar != NULL) && (*nextchars != 0) )
	{
		size_t length = strspn(dollar + 1, envvarnamechars);
		if (length != 0)
		{
			FString varname = FString(dollar + 1, length);
			if (stricmp(varname, "progdir") == 0)
			{
				out += progdir;
			}
			else
			{
				char *varvalue = getenv(varname);
				if ( (varvalue != NULL) && (strlen(varvalue) != 0) )
				{
					out += varvalue;
				}
			}
		}
		else
		{
			out += '$';
		}
		nextchars = dollar + length + 1;
		dollar = strchr(nextchars, '$');
		if (dollar != NULL)
		{
			out += FString(nextchars, dollar - nextchars);
		}
	}
	if (*nextchars != 0)
	{
		out += nextchars;
	}
	return out;
}

//==========================================================================
//
// NicePath
//
// Handles paths with leading ~ characters on Unix as well as environment
// variable substitution. On Windows, this is identical to ExpandEnvVars.
//
//==========================================================================

FString NicePath(const char *path)
{
#ifdef _WIN32
	return ExpandEnvVars(path);
#else
	if (path == NULL || *path == '\0')
	{
		return FString("");
	}
	if (*path != '~')
	{
		return ExpandEnvVars(path);
	}

	passwd *pwstruct;
	const char *slash;

	if (path[1] == '/' || path[1] == '\0')
	{ // Get my home directory
		pwstruct = getpwuid(getuid());
		slash = path + 1;
	}
	else
	{ // Get somebody else's home directory
		slash = strchr(path, '/');
		if (slash == NULL)
		{
			slash = path + strlen(path);
		}
		FString who(path, slash - path);
		pwstruct = getpwnam(who);
	}
	if (pwstruct == NULL)
	{
		return ExpandEnvVars(path);
	}
	FString where(pwstruct->pw_dir);
	if (*slash != '\0')
	{
		where += ExpandEnvVars(slash);
	}
	return where;
#endif
}
