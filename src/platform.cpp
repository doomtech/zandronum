#include "zstring.h"

#ifndef _WIN32

#include <ctype.h>
#include <unistd.h>

// [BB] itoa is not available under Linux, so we supply a replacement here.
// Code taken from http://www.jb.man.ac.uk/~slowe/cpp/itoa.html
/**
 * Ansi C "itoa" based on Kernighan & Ritchie's "Ansi C"
 * with slight modification to optimize for specific architecture:
 */
	
void strreverse(char* begin, char* end) {
	char aux;
	while(end>begin)
		aux=*end, *end--=*begin, *begin++=aux;
}
	
char* itoa(int value, char* str, int base) {
	static char num[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	char* wstr=str;
	int sign;
	div_t res;
	
	// Validate base
	if (base<2 || base>35){ *wstr='\0'; return str; }

	// Take care of sign
	if ((sign=value) < 0) value = -value;

	// Conversion. Number is reversed.
	do {
		res = div(value,base);
		*wstr++ = num[res.rem];
	}while(value=res.quot);
	if(sign<0) *wstr++='-';
	*wstr='\0';

	// Reverse string
	strreverse(str,wstr-1);
	
	return str;
}

/*
**  Portable, public domain replacements for strupr() by Bob Stout
*/

char *strupr(char *string)
{
      char *s;

      if (string)
      {
            for (s = string; *s; ++s)
                  *s = toupper(*s);
      }
      return string;
} 

void I_Sleep( int iMS )
{
	usleep( 1000 * iMS );
}

#endif

/*
#include <iostream>
#include "networkheaders.h"

// [BB] Most of these functions just do nothing if we don't have a GUI
// and therefore are platform independent.
void SERVERCONSOLE_UpdateTitleString( char *pszString ) {}
void SERVERCONSOLE_UpdateScoreboard( void ) {}
void SERVERCONSOLE_UpdateTotalOutboundDataTransfer( LONG lData ) {}
void SERVERCONSOLE_UpdateAverageOutboundDataTransfer( LONG lData ) {}
void SERVERCONSOLE_UpdatePeakOutboundDataTransfer( LONG lData ) {}
void SERVERCONSOLE_UpdateCurrentOutboundDataTransfer( LONG lData ) {}
void SERVERCONSOLE_UpdateTotalInboundDataTransfer( LONG lData ) {}
void SERVERCONSOLE_UpdateAverageInboundDataTransfer( LONG lData ) {}
void SERVERCONSOLE_UpdatePeakInboundDataTransfer( LONG lData ) {}
void SERVERCONSOLE_UpdateCurrentInboundDataTransfer( LONG lData ) {}
void SERVERCONSOLE_UpdateTotalUptime( LONG lData ) {}
void SERVERCONSOLE_SetCurrentMapname( char *pszString ) {}
void SERVERCONSOLE_SetupColumns( void ) {}
void SERVERCONSOLE_AddNewPlayer( LONG lPlayer ) {}
void SERVERCONSOLE_UpdatePlayerInfo( LONG lPlayer, ULONG ulUpdateFlags ) {}
void SERVERCONSOLE_RemovePlayer( LONG lPlayer ) {}
void SERVERCONSOLE_InitializeDMFlagsDisplay( HWND hDlg ) {}
void SERVERCONSOLE_UpdateDMFlagsDisplay( HWND hDlg ) {}
void SERVERCONSOLE_UpdateDMFlags( HWND hDlg ) {}
void SERVERCONSOLE_InitializeLMSSettingsDisplay( HWND hDlg ) {}
void SERVERCONSOLE_UpdateLMSSettingsDisplay( HWND hDlg ) {}
void SERVERCONSOLE_UpdateLMSSettings( HWND hDlg ) {}
void SERVERCONSOLE_InitializeGeneralSettingsDisplay( HWND hDlg ) {}
bool SERVERCONSOLE_IsRestartNeeded( HWND hDlg ) { return false; }
void SERVERCONSOLE_UpdateGeneralSettingsDisplay( HWND hDlg ) {}
void SERVERCONSOLE_UpdateGeneralSettings( HWND hDlg ) {}
void SERVERCONSOLE_UpdateOperatingSystem( char *pszString ) {}
void SERVERCONSOLE_UpdateCPUSpeed( char *pszString ) {}
void SERVERCONSOLE_UpdateVendor( char *pszString ) {}
void SERVERCONSOLE_Print( char *pszString ) { std::cerr << pszString; }
*/
