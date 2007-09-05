//-----------------------------------------------------------------------------
//
// Skulltag Online 2
// Copyright (C) 2007 Rivecoder
// Date created: 8/25/07
//
// Main running class.
//-----------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#	include <wx/wx.h>
#endif 
#include "wxSTO2.h"
#include "version.h"
#include "network.h"
#include "frmMain.h"
#include "log.h"

IMPLEMENT_APP(wxSTO2);

frmMain * mainFrame;

bool wxSTO2::OnInit() 
{
	// Create our main window frame.
	mainFrame = new frmMain(_("Skulltag Online 2: wxWidgets"), wxDefaultPosition, wxSize(500, 300));
	mainFrame->Show(true);
	SetTopWindow(mainFrame);

	NETWORK_Construct(DEFAULT_STO2_PORT, false);
	return true;

}
