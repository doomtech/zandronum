//-----------------------------------------------------------------------------
//
// Skulltag Online 2
// Copyright (C) 2007 Rivecoder
// Date created: 8/27/07
//
// Main GUI code. It uses wxWidgets 2.8.4.
//-----------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#	include <wx/wx.h>
#endif 

#include "wxSTO2.h"
#include "frmMain.h"
#include "sample.xpm"
#include "version.h"
#include "network.h"
#include "log.h"

BEGIN_EVENT_TABLE ( frmMain, wxFrame )
EVT_MENU ( MENU_Quit, frmMain::Quit )
EVT_MENU(MENU_Save, frmMain::SaveFile)
END_EVENT_TABLE()

frmMain::frmMain(const wxString& title, const wxPoint& pos, const wxSize& size) : wxFrame((wxFrame *) NULL, -1, title, pos, size) 
{
	// Set up our controls.
	CreateStatusBar(1);
	SetIcon(sample_xpm);
	this->SetStatusText("Welcome. Everything seems to be OK, but then, what would I know.");
	tbLog = new wxTextCtrl(this, TEXT_Log, "", wxDefaultPosition, wxDefaultSize,  
		wxTE_MULTILINE | wxTE_RICH , wxDefaultValidator, wxTextCtrlNameStr);

	// Set up logging.
	wxLogTextCtrl *MainLog = new wxLogTextCtrl(tbLog);
	wxLog::SetActiveTarget(MainLog);
	LOG_Add("STO2 started. Running revision " SVN_REVISION_STRING ".");

	// Create main menu.
	MainMenu = new wxMenuBar();
	wxMenu *FileMenu = new wxMenu();
	FileMenu->Append(MENU_Save, "&Save log", "Saves the log to a file.");
	FileMenu->Append(MENU_Quit, "&Quit", "Are you sure? Vista is much worse.");
	MainMenu->Append(FileMenu, "&File");
	SetMenuBar(MainMenu);
}
 
void frmMain::SaveFile(wxCommandEvent& WXUNUSED(event))
{
	wxFileDialog* SaveDialog = new wxFileDialog(
		this, _("Choose where to same the log."), wxEmptyString, wxEmptyString, 
		_("Log files (*.log)|*.log|All files|*.*"),
		wxFD_SAVE, wxDefaultPosition);
 
	if (SaveDialog->ShowModal() == wxID_OK)
	{
		tbLog->SaveFile(SaveDialog->GetPath());
		this->SetStatusText("Log saved.");
	}
	else
		this->SetStatusText("Canceled.");
}
 
void frmMain::Quit( wxCommandEvent& event )
{
    Close(TRUE);
}