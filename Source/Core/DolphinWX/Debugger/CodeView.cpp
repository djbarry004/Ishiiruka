// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <wx/brush.h>
#include <wx/chartype.h>
#include <wx/clipbrd.h>
#include <wx/colour.h>
#include <wx/control.h>
#include <wx/dataobj.h>
#include <wx/dcclient.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/menu.h>
#include <wx/menuitem.h>
#include <wx/pen.h>
#include <wx/setup.h>
#include <wx/string.h>
#include <wx/textdlg.h>
#include <wx/translation.h>
#include <wx/windowid.h>

#include "Common/Common.h"
#include "Common/DebugInterface.h"
#include "Common/StringUtil.h"
#include "Common/SymbolDB.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/CodeView.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"

DEFINE_EVENT_TYPE(wxEVT_CODEVIEW_CHANGE);

enum
{
	IDM_GOTOINMEMVIEW = 12000,
	IDM_COPYADDRESS,
	IDM_COPYHEX,
	IDM_COPYCODE,
	IDM_INSERTBLR, IDM_INSERTNOP,
	IDM_RUNTOHERE,
	IDM_JITRESULTS,
	IDM_FOLLOWBRANCH,
	IDM_RENAMESYMBOL,
	IDM_PATCHALERT,
	IDM_COPYFUNCTION,
	IDM_ADDFUNCTION,
};

BEGIN_EVENT_TABLE(CCodeView, wxControl)
EVT_ERASE_BACKGROUND(CCodeView::OnErase)
EVT_PAINT(CCodeView::OnPaint)
EVT_MOUSEWHEEL(CCodeView::OnScrollWheel)
EVT_LEFT_DOWN(CCodeView::OnMouseDown)
EVT_LEFT_UP(CCodeView::OnMouseUpL)
EVT_MOTION(CCodeView::OnMouseMove)
EVT_RIGHT_DOWN(CCodeView::OnMouseDown)
EVT_RIGHT_UP(CCodeView::OnMouseUpR)
EVT_MENU(-1, CCodeView::OnPopupMenu)
EVT_SIZE(CCodeView::OnResize)
END_EVENT_TABLE()

CCodeView::CCodeView(DebugInterface* debuginterface, SymbolDB *symboldb,
wxWindow* parent, wxWindowID Id)
: wxControl(parent, Id),
m_debugger(debuginterface),
m_symbol_db(symboldb),
m_plain(false),
m_curAddress(debuginterface->GetPC()),
m_align(debuginterface->GetInstructionSize(0)),
m_rowHeight(13),
m_selection(0),
m_oldSelection(0),
m_selecting(false),
m_lx(-1),
m_ly(-1)
{
}

int CCodeView::YToAddress(int y)
{
	wxRect rc = GetClientRect();
	int ydiff = y - rc.height / 2 - m_rowHeight / 2;
	ydiff = (int)(floorf((float)ydiff / (float)m_rowHeight)) + 1;
	return m_curAddress + ydiff * m_align;
}

void CCodeView::OnMouseDown(wxMouseEvent& event)
{
	int x = event.m_x;
	int y = event.m_y;

	if (x > 16)
	{
		m_oldSelection = m_selection;
		m_selection = YToAddress(y);
		// SetCapture(wnd);
		bool oldselecting = m_selecting;
		m_selecting = true;

		if (!oldselecting || (m_selection != m_oldSelection))
			Refresh();
	}
	else
	{
		ToggleBreakpoint(YToAddress(y));
	}

	event.Skip();
}

void CCodeView::OnScrollWheel(wxMouseEvent& event)
{
	const bool scroll_down = (event.GetWheelRotation() < 0);
	const int num_lines = event.GetLinesPerAction();

	if (scroll_down)
	{
		m_curAddress += num_lines;
	}
	else
	{
		m_curAddress -= num_lines;
	}

	Refresh();
	event.Skip();
}

void CCodeView::ToggleBreakpoint(u32 address)
{
	m_debugger->ToggleBreakpoint(address);
	Refresh();
	Host_UpdateBreakPointView();
}

void CCodeView::OnMouseMove(wxMouseEvent& event)
{
	wxRect rc = GetClientRect();

	if (event.m_leftDown && event.m_x > 16)
	{
		if (event.m_y < 0)
		{
			m_curAddress -= m_align;
			Refresh();
		}
		else if (event.m_y > rc.height)
		{
			m_curAddress += m_align;
			Refresh();
		}
		else
		{
			OnMouseDown(event);
		}
	}

	event.Skip();
}

void CCodeView::RaiseEvent()
{
	wxCommandEvent ev(wxEVT_CODEVIEW_CHANGE, GetId());
	ev.SetEventObject(this);
	ev.SetInt(m_selection);
	GetEventHandler()->ProcessEvent(ev);
}

void CCodeView::OnMouseUpL(wxMouseEvent& event)
{
	if (event.m_x > 16)
	{
		m_curAddress = YToAddress(event.m_y);
		m_selecting = false;
		Refresh();
	}

	RaiseEvent();
	event.Skip();
}

u32 CCodeView::AddrToBranch(u32 addr)
{
	char disasm[256];
	m_debugger->Disassemble(addr, disasm, 256);
	const char *mojs = strstr(disasm, "->0x");
	if (mojs)
	{
		u32 dest;
		sscanf(mojs + 4, "%08x", &dest);
		return dest;
	}
	return 0;
}

void CCodeView::InsertBlrNop(int Blr)
{
	// Check if this address has been modified
	int find = -1;
	for (u32 i = 0; i < m_blrList.size(); i++)
	{
		if (m_blrList.at(i).address == m_selection)
		{
			find = i;
			break;
		}
	}

	// Save the old value
	if (find >= 0)
	{
		m_debugger->WriteExtraMemory(0, m_blrList.at(find).oldValue, m_selection);
		m_blrList.erase(m_blrList.begin() + find);
	}
	else
	{
		BlrStruct temp;
		temp.address = m_selection;
		temp.oldValue = m_debugger->ReadMemory(m_selection);
		m_blrList.push_back(temp);
		if (Blr == 0)
			m_debugger->InsertBLR(m_selection, 0x4e800020);
		else
			m_debugger->InsertBLR(m_selection, 0x60000000);
	}
	Refresh();
}

void CCodeView::OnPopupMenu(wxCommandEvent& event)
{
#if wxUSE_CLIPBOARD
	wxTheClipboard->Open();
#endif

	switch (event.GetId())
	{
	case IDM_GOTOINMEMVIEW:
		// CMemoryDlg::Goto(selection);
		break;

#if wxUSE_CLIPBOARD
	case IDM_COPYADDRESS:
		wxTheClipboard->SetData(new wxTextDataObject(wxString::Format("%08x", m_selection)));
		break;

	case IDM_COPYCODE:
	{
		char disasm[256];
		m_debugger->Disassemble(m_selection, disasm, 256);
		wxTheClipboard->SetData(new wxTextDataObject(StrToWxStr(disasm)));
	}
		break;

	case IDM_COPYHEX:
	{
		char temp[24];
		sprintf(temp, "%08x", m_debugger->ReadInstruction(m_selection));
		wxTheClipboard->SetData(new wxTextDataObject(StrToWxStr(temp)));
	}
		break;


	case IDM_COPYFUNCTION:
	{
		Symbol *symbol = m_symbol_db->GetSymbolFromAddr(m_selection);
		if (symbol)
		{
			std::string text;
			text = text + symbol->name + "\r\n";
			// we got a function
			u32 start = symbol->address;
			u32 end = start + symbol->size;
			for (u32 addr = start; addr != end; addr += 4)
			{
				char disasm[256];
				m_debugger->Disassemble(addr, disasm, 256);
				text = text + StringFromFormat("%08x: ", addr) + disasm + "\r\n";
			}
			wxTheClipboard->SetData(new wxTextDataObject(StrToWxStr(text)));
		}
	}
		break;
#endif

	case IDM_RUNTOHERE:
		m_debugger->SetBreakpoint(m_selection);
		m_debugger->RunToBreakpoint();
		Refresh();
		break;

		// Insert blr or restore old value
	case IDM_INSERTBLR:
		InsertBlrNop(0);
		Refresh();
		break;
	case IDM_INSERTNOP:
		InsertBlrNop(1);
		Refresh();
		break;

	case IDM_JITRESULTS:
		m_debugger->ShowJitResults(m_selection);
		break;

	case IDM_FOLLOWBRANCH:
	{
		u32 dest = AddrToBranch(m_selection);
		if (dest)
		{
			Center(dest);
			RaiseEvent();
		}
	}
		break;

	case IDM_ADDFUNCTION:
		m_symbol_db->AddFunction(m_selection);
		Host_NotifyMapLoaded();
		break;

	case IDM_RENAMESYMBOL:
	{
		Symbol *symbol = m_symbol_db->GetSymbolFromAddr(m_selection);
		if (symbol)
		{
			wxTextEntryDialog input_symbol(this, _("Rename symbol:"),
				wxGetTextFromUserPromptStr,
				StrToWxStr(symbol->name));
			if (input_symbol.ShowModal() == wxID_OK)
			{
				symbol->name = WxStrToStr(input_symbol.GetValue());
				Refresh(); // Redraw to show the renamed symbol
			}
			Host_NotifyMapLoaded();
		}
	}
		break;

	case IDM_PATCHALERT:
		break;
	}

#if wxUSE_CLIPBOARD
	wxTheClipboard->Close();
#endif
	event.Skip();
}

void CCodeView::OnMouseUpR(wxMouseEvent& event)
{
	bool isSymbol = m_symbol_db->GetSymbolFromAddr(m_selection) != nullptr;
	// popup menu
	wxMenu* menu = new wxMenu;
	//menu->Append(IDM_GOTOINMEMVIEW, "&Goto in mem view");
	menu->Append(IDM_FOLLOWBRANCH, _("&Follow branch"))->Enable(AddrToBranch(m_selection) ? true : false);
	menu->AppendSeparator();
#if wxUSE_CLIPBOARD
	menu->Append(IDM_COPYADDRESS, _("Copy &address"));
	menu->Append(IDM_COPYFUNCTION, _("Copy &function"))->Enable(isSymbol);
	menu->Append(IDM_COPYCODE, _("Copy &code line"));
	menu->Append(IDM_COPYHEX, _("Copy &hex"));
	menu->AppendSeparator();
#endif
	menu->Append(IDM_RENAMESYMBOL, _("Rename &symbol"))->Enable(isSymbol);
	menu->AppendSeparator();
	menu->Append(IDM_RUNTOHERE, _("&Run To Here"))->Enable(Core::IsRunning());
	menu->Append(IDM_ADDFUNCTION, _("&Add function"))->Enable(Core::IsRunning());
	menu->Append(IDM_JITRESULTS, _("PPC vs X86"))->Enable(Core::IsRunning());
	menu->Append(IDM_INSERTBLR, _("Insert &blr"))->Enable(Core::IsRunning());
	menu->Append(IDM_INSERTNOP, _("Insert &nop"))->Enable(Core::IsRunning());
	menu->Append(IDM_PATCHALERT, _("Patch alert"))->Enable(Core::IsRunning());
	PopupMenu(menu);
	event.Skip();
}

void CCodeView::OnErase(wxEraseEvent& event)
{}

void CCodeView::OnPaint(wxPaintEvent& event)
{
	// --------------------------------------------------------------------
	// General settings
	// -------------------------
	wxPaintDC dc(this);
	wxRect rc = GetClientRect();

	dc.SetFont(DebuggerFont);

	wxCoord w, h;
	dc.GetTextExtent("0WJyq", &w, &h);

	if (h > m_rowHeight)
		m_rowHeight = h;

	dc.GetTextExtent("W", &w, &h);
	int charWidth = w;

	struct branch
	{
		int src, dst, srcAddr;
	};

	branch branches[256];
	int numBranches = 0;
	// TODO: Add any drawing code here...
	int width = rc.width;
	int numRows = (rc.height / m_rowHeight) / 2 + 2;
	// ------------

	// --------------------------------------------------------------------
	// Colors and brushes
	// -------------------------
	dc.SetBackgroundMode(wxTRANSPARENT); // the text background
	const wxColour bgColor = *wxWHITE;
	wxPen nullPen(bgColor);
	wxPen currentPen(*wxBLACK_PEN);
	wxPen selPen(*wxGREY_PEN);
	nullPen.SetStyle(wxTRANSPARENT);
	currentPen.SetStyle(wxSOLID);
	wxBrush currentBrush(*wxLIGHT_GREY_BRUSH);
	wxBrush pcBrush(*wxGREEN_BRUSH);
	wxBrush bpBrush(*wxRED_BRUSH);

	wxBrush bgBrush(bgColor);
	wxBrush nullBrush(bgColor);
	nullBrush.SetStyle(wxTRANSPARENT);

	dc.SetPen(nullPen);
	dc.SetBrush(bgBrush);
	dc.DrawRectangle(0, 0, 16, rc.height);
	dc.DrawRectangle(0, 0, rc.width, 5);
	// ------------

	// --------------------------------------------------------------------
	// Walk through all visible rows
	// -------------------------
	for (int i = -numRows; i <= numRows; i++)
	{
		unsigned int address = m_curAddress + i * m_align;

		int rowY1 = rc.height / 2 + m_rowHeight * i - m_rowHeight / 2;
		int rowY2 = rc.height / 2 + m_rowHeight * i + m_rowHeight / 2;

		wxString temp = wxString::Format("%08x", address);
		u32 col = m_debugger->GetColor(address);
		wxBrush rowBrush(wxColour(col >> 16, col >> 8, col));
		dc.SetBrush(nullBrush);
		dc.SetPen(nullPen);
		dc.DrawRectangle(0, rowY1, 16, rowY2 - rowY1 + 2);

		if (m_selecting && (address == m_selection))
			dc.SetPen(selPen);
		else
			dc.SetPen(i == 0 ? currentPen : nullPen);

		if (address == m_debugger->GetPC())
			dc.SetBrush(pcBrush);
		else
			dc.SetBrush(rowBrush);

		dc.DrawRectangle(16, rowY1, width, rowY2 - rowY1 + 1);
		dc.SetBrush(currentBrush);
		if (!m_plain)
		{
			dc.SetTextForeground("#600000"); // the address text is dark red
			dc.DrawText(temp, 17, rowY1);
			dc.SetTextForeground(*wxBLACK);
		}

		// If running
		if (m_debugger->IsAlive())
		{
			char dis[256];
			m_debugger->Disassemble(address, dis, 256);
			char* dis2 = strchr(dis, '\t');
			char desc[256] = "";

			// If we have a code
			if (dis2)
			{
				*dis2 = 0;
				dis2++;
				// look for hex strings to decode branches
				const char* mojs = strstr(dis2, "0x8");
				if (mojs)
				{
					for (int k = 0; k < 8; k++)
					{
						bool found = false;
						for (int j = 0; j < 22; j++)
						{
							if (mojs[k + 2] == "0123456789ABCDEFabcdef"[j])
								found = true;
						}
						if (!found)
						{
							mojs = nullptr;
							break;
						}
					}
				}
				if (mojs)
				{
					int offs;
					sscanf(mojs + 2, "%08x", &offs);
					branches[numBranches].src = rowY1 + m_rowHeight / 2;
					branches[numBranches].srcAddr = address / m_align;
					branches[numBranches++].dst = (int)(rowY1 + ((s64)(u32)offs - (s64)(u32)address) * m_rowHeight / m_align + m_rowHeight / 2);
					sprintf(desc, "-->%s", m_debugger->GetDescription(offs).c_str());
					dc.SetTextForeground(wxTheColourDatabase->Find("PURPLE")); // the -> arrow illustrations are purple
				}
				else
				{
					dc.SetTextForeground(*wxBLACK);
				}

				dc.DrawText(StrToWxStr(dis2), 17 + 17 * charWidth, rowY1);
				// ------------
			}

			// Show blr as its' own color
			if (strcmp(dis, "blr"))
				dc.SetTextForeground(wxTheColourDatabase->Find("DARK GREEN"));
			else
				dc.SetTextForeground(wxTheColourDatabase->Find("VIOLET"));

			dc.DrawText(StrToWxStr(dis), 17 + (m_plain ? 1 * charWidth : 9 * charWidth), rowY1);

			if (desc[0] == 0)
			{
				strcpy(desc, m_debugger->GetDescription(address).c_str());
			}

			if (!m_plain)
			{
				dc.SetTextForeground(*wxBLUE);

				//char temp[256];
				//UnDecorateSymbolName(desc,temp,255,UNDNAME_COMPLETE);
				if (strlen(desc))
				{
					dc.DrawText(StrToWxStr(desc), 17 + 35 * charWidth, rowY1);
				}
			}

			// Show red breakpoint dot
			if (m_debugger->IsBreakpoint(address))
			{
				dc.SetBrush(bpBrush);
				dc.DrawRectangle(2, rowY1 + 1, 11, 11);
			}
		}
	} // end of for
	// ------------

	// --------------------------------------------------------------------
	// Colors and brushes
	// -------------------------
	dc.SetPen(currentPen);

	for (int i = 0; i < numBranches; i++)
	{
		int x = 17 + 49 * charWidth + (branches[i].srcAddr % 9) * 8;
		MoveTo(x - 2, branches[i].src);

		if (branches[i].dst < rc.height + 400 && branches[i].dst > -400)
		{
			LineTo(dc, x + 2, branches[i].src);
			LineTo(dc, x + 2, branches[i].dst);
			LineTo(dc, x - 4, branches[i].dst);

			MoveTo(x, branches[i].dst - 4);
			LineTo(dc, x - 4, branches[i].dst);
			LineTo(dc, x + 1, branches[i].dst + 5);
		}
		//else
		//{
		// This can be re-enabled when there is a scrollbar or
		// something on the codeview (the lines are too long)

		//LineTo(dc, x+4, branches[i].src);
		//MoveTo(x+2, branches[i].dst-4);
		//LineTo(dc, x+6, branches[i].dst);
		//LineTo(dc, x+1, branches[i].dst+5);
		//}

		//LineTo(dc, x, branches[i].dst+4);
		//LineTo(dc, x-2, branches[i].dst);
	}
	// ------------
}

void CCodeView::LineTo(wxPaintDC &dc, int x, int y)
{
	dc.DrawLine(m_lx, m_ly, x, y);
	m_lx = x;
	m_ly = y;
}

void CCodeView::OnResize(wxSizeEvent& event)
{
	Refresh();
	event.Skip();
}
