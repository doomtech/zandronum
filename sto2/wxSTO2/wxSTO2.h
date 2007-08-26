//-----------------------------------------------------------------------------
//
// Skulltag Online 2
// Copyright (C) 2007 Rivecoder
// Date created: 8/25/07
//
//-----------------------------------------------------------------------------

#ifndef __BASE_H 
#define __BASE_H 
 
class wxSTO2 : public wxApp
{
public:
	virtual bool OnInit();
};

class STO2Frame : public wxFrame
{
public:
	STO2Frame(const wxString& title, const wxPoint& pos, const wxSize& size); 
	
	wxTextCtrl	*tbLog;
	wxMenuBar	*MainMenu;

	void Quit( wxCommandEvent& event );
    void SaveFile(wxCommandEvent& event);
    DECLARE_EVENT_TABLE()
};

enum
{
  TEXT_Log = wxID_HIGHEST + 1,
  MENU_Save,
  MENU_Quit
};

#endif 