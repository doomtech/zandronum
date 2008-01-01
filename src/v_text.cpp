/*
** v_text.cpp
** Draws text to a canvas. Also has a text line-breaker thingy.
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

#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "v_text.h"

#include "i_system.h"
#include "v_video.h"
#include "hu_stuff.h"
#include "w_wad.h"
#include "m_swap.h"

#include "doomstat.h"
#include "templates.h"

#include "sv_main.h"

//
// SetFont
//
// Set the canvas's font
//
void DCanvas::SetFont (FFont *font)
{
	Font = font;
}

void STACK_ARGS DCanvas::DrawChar (int normalcolor, int x, int y, BYTE character, ...)
{
	if (Font == NULL)
		return;

	if (normalcolor >= NumTextColors)
		normalcolor = CR_UNTRANSLATED;

	FTexture *pic;
	int dummy;

	if (NULL != (pic = Font->GetChar (character, &dummy)))
	{
		const FRemapTable *range = Font->GetColorTranslation ((EColorRange)normalcolor);
		va_list taglist;
		va_start (taglist, character);
		DrawTexture (pic, x, y, DTA_Translation, range, TAG_MORE, &taglist);
		va_end (taglist);
	}
}

//
// DrawText
//
// Write a string using the current font
//
void STACK_ARGS DCanvas::DrawText (int normalcolor, int x, int y, const char *string, ...)
{
	va_list tags;
	DWORD tag;
	INTBOOL boolval;

	int			maxstrlen = INT_MAX;
	int 		w, maxwidth;
	const BYTE *ch;
	int 		c;
	int 		cx;
	int 		cy;
	int			boldcolor;
	const FRemapTable *range;
	int			height;
	int			forcedwidth = 0;
	int			scalex, scaley;
	int			kerning;
	FTexture *pic;

	if (Font == NULL || string == NULL)
		return;

	if (normalcolor >= NumTextColors)
		normalcolor = CR_UNTRANSLATED;
	boldcolor = normalcolor ? normalcolor - 1 : NumTextColors - 1;

	range = Font->GetColorTranslation ((EColorRange)normalcolor);
	height = Font->GetHeight () + 1;
	kerning = Font->GetDefaultKerning ();

	ch = (const BYTE *)string;
	cx = x;
	cy = y;

	// Parse the tag list to see if we need to adjust for scaling.
 	maxwidth = Width;
	scalex = scaley = 1;

	va_start (tags, string);
	tag = va_arg (tags, DWORD);

	while (tag != TAG_DONE)
	{
		va_list *more_p;
		DWORD data;
		void *ptrval;

		switch (tag)
		{
		case TAG_IGNORE:
		default:
			data = va_arg (tags, DWORD);
			break;

		case TAG_MORE:
			more_p = va_arg (tags, va_list*);
			va_end (tags);
#ifdef __GNUC__
			__va_copy (tags, *more_p);
#else
			tags = *more_p;
#endif
			break;

		// We don't handle these. :(
		case DTA_DestWidth:
		case DTA_DestHeight:
			*(DWORD *)tags = TAG_IGNORE;
			data = va_arg (tags, DWORD);
			break;

		// Translation is specified explicitly by the text.
		case DTA_Translation:
			*(DWORD *)tags = TAG_IGNORE;
			ptrval = va_arg (tags, void*);
			break;

		case DTA_CleanNoMove:
			boolval = va_arg (tags, INTBOOL);
			if (boolval)
			{
				scalex = (LONG)CleanXfac;
				scaley = (LONG)CleanYfac;
				maxwidth = Width - (Width % (int)CleanYfac);
			}
			break;

		case DTA_Clean:
		case DTA_320x200:
			boolval = va_arg (tags, INTBOOL);
			if (boolval)
			{
				scalex = scaley = 1;
				maxwidth = 320;
			}
			break;

		case DTA_VirtualWidth:
			maxwidth = va_arg (tags, int);
			scalex = scaley = 1;
			break;

		case DTA_TextLen:
			maxstrlen = va_arg (tags, int);
			break;

		case DTA_CellX:
			forcedwidth = va_arg (tags, int);
			break;

		case DTA_CellY:
			height = va_arg (tags, int);
			break;

		// [BC] Is this text? If so, handle it slightly differently when we draw it.
		case DTA_IsText:

			va_arg( tags, int );
			break;
		}
		tag = va_arg (tags, DWORD);
	}

	height *= scaley;
		
	while ((const char *)ch - string < maxstrlen)
	{
		c = *ch++;
		if (!c)
			break;

		if (c == TEXTCOLOR_ESCAPE)
		{
			EColorRange newcolor = V_ParseFontColor (ch, normalcolor, boldcolor);
			if (newcolor != CR_UNDEFINED)
			{
				range = Font->GetColorTranslation (newcolor);
			}
			continue;
		}
		
		if (c == '\n')
		{
			cx = x;
			cy += height;
			continue;
		}

		if (NULL != (pic = Font->GetChar (c, &w)))
		{
			va_list taglist;
			va_start (taglist, string);
			// [BC] Flag this as being text.
			if (forcedwidth)
			{
				w = forcedwidth;
				DrawTexture (pic, cx, cy,
					DTA_Translation, range,
					DTA_DestWidth, forcedwidth,
					DTA_DestHeight, height,
					DTA_IsText, true,
					TAG_MORE, &taglist);
			}
			else
			{
				DrawTexture (pic, cx, cy,
					DTA_Translation, range,
					DTA_IsText, true,
					TAG_MORE, &taglist);
			}
			va_end (taglist);
		}
		cx += (w + kerning) * scalex;
	}
}

//
// Find string width using this font
//
int FFont::StringWidth (const BYTE *string) const
{
	int w = 0;
	int maxw = 0;
		
	while (*string)
	{
		if (*string == TEXTCOLOR_ESCAPE)
		{
			++string;
			if (*string == '[')
			{
				while (*string != '\0' && *string != ']')
				{
					++string;
				}
			}
			else if (*string != '\0')
			{
				++string;
			}
			continue;
		}
		else if (*string == '\n')
		{
			if (w > maxw)
				maxw = w;
			w = 0;
			++string;
		}
		else
		{
			w += GetCharWidth (*string++) + GlobalKerning;
		}
	}
				
	return MAX (maxw, w);
}

// [BC] This is essentially strbin() from the original ZDoom code. It's been made a public
// function so that we can colorize lots of things.
void V_ColorizeString( char *pszString )
{
	char *p = pszString, c;
	int i;

	while ( (c = *p++) )
	{
		// If we don't encounter a slash, keep parsing.
		if (c != '\\')
			*pszString++ = c;
		else
		{
			switch (*p)
			{
				case 'c':
					*pszString++ = TEXTCOLOR_ESCAPE;
					break;
				case 'n':
					*pszString++ = '\n';
					break;
				case 't':
					*pszString++ = '\t';
					break;
				case 'r':
					*pszString++ = '\r';
					break;
//				case '\n':
//					break;
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
					*pszString++ = c;
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
					*pszString++ = c;
					break;
				default:
					*pszString++ = *p;
					break;
			}
			p++;
		}
	}
	*pszString = 0;
}

// [BB] Version of V_ColorizeString that accepts a FString as argument.
// Hacks like "V_ColorizeString( (char *)g_MOTD.GetChars( ));"
// are not needed anymore using this.
void V_ColorizeString( FString &String )
{
	const int length = (int) String.Len();
	char *tempCharArray = new char[length+1];
	strncpy( tempCharArray, String.GetChars(), length );
	tempCharArray[length] = 0;
	V_ColorizeString( tempCharArray );
	String = tempCharArray;
	delete[] tempCharArray;
}

// [BC] This essentially does the reverse of V_ColorizeString(). It takes a string with
// color codes and puts it back in \c<color code> format.
void V_UnColorizeString( char *pszString, ULONG ulMaxStringLength )
{
	char	*p;
	char	c;
	ULONG	ulCurStringLength;

	ulCurStringLength = static_cast<ULONG>(strlen( pszString ));

	p = pszString;
	while ( (c = *p++) )
	{
		if ( c == TEXTCOLOR_ESCAPE )
		{
			ULONG	ulPos;

			ulCurStringLength++;
			if ( ulCurStringLength > ulMaxStringLength )
			{
				pszString++;
				continue;
			}

			for ( ulPos = static_cast<ULONG>(strlen( pszString ) + 1); ulPos > 0; ulPos-- )
				pszString[ulPos] = pszString[ulPos - 1];

			pszString[0] = '\\';
			pszString[1] = 'c';
		}

		pszString++;
	}
	*pszString = 0;
}

// [BC] This strips color codes from a string.
void V_RemoveColorCodes( char *pszString )
{
	char	*p;
	char	c;
	char	*pszEnd;

	// Start at the beginning of the string.
	p = pszString;

	// Look at the current character.
	while ( (c = *p) )
	{
		// If this is a color character, remove it along with the color code from the string.
		if ( c == TEXTCOLOR_ESCAPE )
		{
			pszEnd = p + 1;

			// This character is the color code. Ones that start with a left bracket are
			// multiple characters and end with the right bracket.
			switch ( *pszEnd )
			{
			case '[':

				while (( *pszEnd != ']' ) && ( *pszEnd != 0 ))
					pszEnd++;
				break;
			default:

				break;
			}

			if ( *pszEnd != 0 )
				pszEnd++;

			// Make sure we copy the terminating null character, too.
			memcpy( p, pszEnd, strlen( pszEnd ) + 1 );
		}
		// Otherwise, look at the next character.
		else
			p++;
	}
}

// [RC] Strips color codes from an FString.
void V_RemoveColorCodes( FString &String )
{
	const ULONG		length = (ULONG) String.Len();
	char			*szString = new char[length+1];
	
	// Copy the FString to a temporary char array.
	strncpy( szString, String.GetChars(), length );
	szString[length] = 0;

	// Remove the colors.
	V_ColorizeString( szString );
	V_RemoveColorCodes( szString );

	// Convert back and clean up.
	String = szString;
	delete[] szString;
}

// [BB] Strips color codes from a string respecting sv_colorstripmethod.
void V_StripColors( char *pszString )
{
	switch ( sv_colorstripmethod )
	{
	// Strip messages here of color codes.
	case 0:

		V_ColorizeString( pszString );
		V_RemoveColorCodes( pszString );
		break;
	// Don't strip out the color codes.
	case 1:

		V_ColorizeString( pszString );
		break;
	// Just leave the damn thing alone!
	case 2:
	default:

		break;
	}
}

// [RC] Returns whether this character is allowed in names.
bool v_IsCharAcceptableInNames ( char c )
{
	// Allow color codes.
	if ( c == 28 )
		return true;

	// No undisplayable system ascii.
	if ( c <= 31 )
		return false;

	// Percent is forbiddon in net names (why?).
	if ( c == '%' )
		return false;

	// Pound is hard to type on USA boards.
	if ( c == 38 )
		return false;

	// No escape codes (\c is handled differently).
	if ( c == 92 )
		return false;

	// Tilde is invisible.
	if ( c == 96 )
		return false;

	// Ascii above 123 is invisible or hard to type.
	if ( c >= 123 )
		return false;

	return true;
}

// [RC] Returns whether this character is invisible.
bool v_IsCharacterWhitespace ( char c )
{
	// System ascii < 32 is invisible.
	if ( c <= 31 )
		return true;

	// Space.
	if ( c == 32 )
		return true;

	// Delete.
	if ( c == 127 )
		return true;

	// Ascii 255.
	if ( c == 255 )
		return true;

	return false;
}


// [RC] Conforms names to meet standards.
void V_CleanPlayerName( char *pszString )
{
	char	*p;
	char	c;
	ULONG	ulStringLength;
	ULONG   ulTotalLength;
	ULONG   ulNonWhitespace;

	ulStringLength = static_cast<ULONG>(strlen( pszString ));
	ulTotalLength = 0;
	ulNonWhitespace = 0;

	// Start at the beginning of the string.
	p = pszString;

	// The name must be longer than three characters.
	if ( ulStringLength < 3 )
	{
		strcpy( pszString, "Player" );
		return;
	}

	// Go through and remove the illegal characters.
	while ( (c = *p++) )
	{
		if ( !v_IsCharAcceptableInNames(c) )
		{
			ULONG	ulPos;
			ulStringLength = static_cast<ULONG>(strlen( pszString ));

			// Shift the rest of the string back one.
			for ( ulPos = 0; ulPos < ulStringLength; ulPos++ )
				pszString[ulPos] = pszString[ulPos + 1];

			// Don't skip a character.
			p--;
		}
		else
		{
			pszString++;

			if ( !v_IsCharacterWhitespace(c) )
				ulNonWhitespace++;
			ulTotalLength++;
		}
	}

	// Cut the string at its new end.
	*pszString = 0;
		
	// Check the length again, as characters were removed.
	if(ulNonWhitespace < 3)
	{
		pszString -= ulTotalLength;
		strcpy( pszString, "Player" );
	}
}

// [RC] Converts COL_ numbers to their \c counterparts.
char V_GetColorChar( ULONG ulColor )
{
	return (char) ( 97 + (int) ulColor );
}

//
// Break long lines of text into multiple lines no longer than maxwidth pixels
//
static void breakit (FBrokenLines *line, FFont *font, const BYTE *start, const BYTE *stop, FString &linecolor)
{
	if (!linecolor.IsEmpty())
	{
		line->Text = TEXTCOLOR_ESCAPE;
		line->Text += linecolor;
	}
	line->Text.AppendCStrPart ((const char *)start, stop - start);
	line->Width = font->StringWidth (line->Text);
}

FBrokenLines *V_BreakLines (FFont *font, int maxwidth, const BYTE *string)
{
	FBrokenLines lines[128];	// Support up to 128 lines (should be plenty)

	const BYTE *space = NULL, *start = string;
	int i, c, w, nw;
	FString lastcolor, linecolor;
	bool lastWasSpace = false;
	int kerning = font->GetDefaultKerning ();

	i = w = 0;

	while ( (c = *string++) && i < 128 )
	{
		if (c == TEXTCOLOR_ESCAPE)
		{
			if (*string)
			{
				if (*string == '[')
				{
					const BYTE *start = string;
					while (*string != ']' && *string != '\0')
					{
						string++;
					}
					if (*string != '\0')
					{
						string++;
					}
					lastcolor = FString((const char *)start, string - start);
				}
				else
				{
					lastcolor = *string++;
				}
			}
			continue;
		}

		if (isspace(c)) 
		{
			if (!lastWasSpace)
			{
				space = string - 1;
				lastWasSpace = true;
			}
		}
		else
		{
			lastWasSpace = false;
		}

		nw = font->GetCharWidth (c);

		if ((w > 0 && w + nw > maxwidth) || c == '\n')
		{ // Time to break the line
			if (!space)
				space = string - 1;

			breakit (&lines[i], font, start, space, linecolor);
			if (c == '\n')
			{
				lastcolor = "";		// Why, oh why, did I do it like this?
			}
			linecolor = lastcolor;

			i++;
			w = 0;
			lastWasSpace = false;
			start = space;
			space = NULL;

			while (*start && isspace (*start) && *start != '\n')
				start++;
			if (*start == '\n')
				start++;
			else
				while (*start && isspace (*start))
					start++;
			string = start;
		}
		else
		{
			w += nw + kerning;
		}
	}

	// String here is pointing one character after the '\0'
	if (i < 128 && --string - start >= 1)
	{
		const BYTE *s = start;

		while (s < string)
		{
			// If there is any non-white space in the remainder of the string, add it.
			if (!isspace (*s++))
			{
				breakit (&lines[i++], font, start, string, linecolor);
				break;
			}
		}
	}

	// Make a copy of the broken lines and return them
	FBrokenLines *broken = new FBrokenLines[i+1];

	for (c = 0; c < i; ++c)
	{
		broken[c] = lines[c];
	}
	broken[c].Width = -1;

	return broken;
}

void V_FreeBrokenLines (FBrokenLines *lines)
{
	if (lines)
	{
		delete[] lines;
	}
}
