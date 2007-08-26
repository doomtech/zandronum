//-----------------------------------------------------------------------------
//
// Skulltag Online 2
// Copyright (C) 2007 Rivecoder
// Date created: 8/25/07
//
// Main GUI code. It uses wxWidgets 2.8.4.
//-----------------------------------------------------------------------------

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#	include <wx/wx.h>
#endif 
#include "wxSTO2.h"
#include "sample.xpm"
#include "version.h"

IMPLEMENT_APP(wxSTO2);
BEGIN_EVENT_TABLE ( STO2Frame, wxFrame )
EVT_MENU ( MENU_Quit, STO2Frame::Quit )
EVT_MENU(MENU_Save, STO2Frame::SaveFile)
END_EVENT_TABLE()

bool wxSTO2::OnInit() 
{
	// Create our main window frame.
	STO2Frame *MainWin = new STO2Frame(_("Skulltag Online 2: wxWidgets"), wxDefaultPosition, wxSize(500, 300));
	MainWin->Show(true);
	SetTopWindow(MainWin);
	return true;
} 
 
STO2Frame::STO2Frame(const wxString& title, const wxPoint& pos, const wxSize& size) : wxFrame((wxFrame *) NULL, -1, title, pos, size) 
{
	SetIcon(sample_xpm);

	// Set up our controls.
	CreateStatusBar(1);
	this->SetStatusText("Welcome. Everything seems to be OK, but then, what would I know.");
	tbLog = new wxTextCtrl(this, TEXT_Log, "", wxDefaultPosition, wxDefaultSize,  
		wxTE_MULTILINE | wxTE_RICH , wxDefaultValidator, wxTextCtrlNameStr);

	*tbLog << "STO2 started. Running revision " SVN_REVISION_STRING ".\n";

	// Create main menu.
	MainMenu = new wxMenuBar();
	wxMenu *FileMenu = new wxMenu();
	FileMenu->Append(MENU_Save, "&Save log", "Saves the log to a file.");
	FileMenu->Append(MENU_Quit, "&Quit", "Are you sure? Vista is much worse.");
	MainMenu->Append(FileMenu, "&File");
	SetMenuBar(MainMenu);
}
 
void STO2Frame::SaveFile(wxCommandEvent& WXUNUSED(event))
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
 
void STO2Frame::Quit( wxCommandEvent& event )
{
    Close(TRUE);
}