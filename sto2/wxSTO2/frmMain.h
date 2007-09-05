//-----------------------------------------------------------------------------
//
// Skulltag Online 2
// Copyright (C) 2007 Rivecoder
// Date created: 8/27/07
//
// Main GUI code. It uses wxWidgets 2.8.4.
//-----------------------------------------------------------------------------

#ifndef __FRMMAIN_H_
#define __FRMMAIN_H_

class frmMain : public wxFrame
{
public:
	frmMain(const wxString& title, const wxPoint& pos, const wxSize& size); 
	
	wxTextCtrl	* tbLog;
	wxMenuBar	* MainMenu;
	void Quit(wxCommandEvent& event);
    void SaveFile(wxCommandEvent& event);
	void AddToLogBox(wxString message);
    DECLARE_EVENT_TABLE()
};

enum
{
  TEXT_Log = wxID_HIGHEST + 1,
  MENU_Save,
  MENU_Quit
};

#endif