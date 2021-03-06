// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <wx/brush.h>
#include <wx/chartype.h>
#include <wx/clipbrd.h>
#include <wx/colour.h>
#include <wx/control.h>
#include <wx/dataobj.h>
#include <wx/dcclient.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/font.h>
#include <wx/gdicmn.h>
#include <wx/menu.h>
#include <wx/pen.h>
#include <wx/setup.h>
#include <wx/string.h>
#include <wx/window.h>

#include "Common/Common.h"
#include "Common/DebugInterface.h"
#include "Core/Host.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"
#include "DolphinWX/Debugger/MemoryView.h"

enum
{
	IDM_GOTOINMEMVIEW = 12000,
	IDM_COPYADDRESS,
	IDM_COPYHEX,
	IDM_COPYCODE,
	IDM_RUNTOHERE,
	IDM_DYNARECRESULTS,
	IDM_TOGGLEMEMORY,
	IDM_VIEWASFP,
	IDM_VIEWASASCII,
	IDM_VIEWASHEX,
};

BEGIN_EVENT_TABLE(CMemoryView, wxControl)
EVT_PAINT(CMemoryView::OnPaint)
EVT_LEFT_DOWN(CMemoryView::OnMouseDownL)
EVT_LEFT_UP(CMemoryView::OnMouseUpL)
EVT_MOTION(CMemoryView::OnMouseMove)
EVT_RIGHT_DOWN(CMemoryView::OnMouseDownR)
EVT_MOUSEWHEEL(CMemoryView::OnScrollWheel)
EVT_MENU(-1, CMemoryView::OnPopupMenu)
EVT_SIZE(CMemoryView::OnResize)
END_EVENT_TABLE()

CMemoryView::CMemoryView(DebugInterface* debuginterface, wxWindow* parent)
: wxControl(parent, wxID_ANY)
, curAddress(debuginterface->GetPC())
, debugger(debuginterface)
, align(debuginterface->GetInstructionSize(0))
, rowHeight(13)
, selection(0)
, oldSelection(0)
, selecting(false)
, memory(0)
, viewAsType(VIEWAS_FP)
{
}

int CMemoryView::YToAddress(int y)
{
	wxRect rc = GetClientRect();
	int ydiff = y - rc.height / 2 - rowHeight / 2;
	ydiff = (int)(floorf((float)ydiff / (float)rowHeight)) + 1;
	return curAddress + ydiff * align;
}

void CMemoryView::OnMouseDownL(wxMouseEvent& event)
{
	int x = event.m_x;
	int y = event.m_y;

	if (x > 16)
	{
		oldSelection = selection;
		selection = YToAddress(y);
		bool oldselecting = selecting;
		selecting = true;

		if (!oldselecting || (selection != oldSelection))
			Refresh();
	}
	else
	{
		debugger->ToggleMemCheck(YToAddress(y));

		Refresh();
		Host_UpdateBreakPointView();
	}

	event.Skip();
}

void CMemoryView::OnMouseMove(wxMouseEvent& event)
{
	wxRect rc = GetClientRect();

	if (event.m_leftDown && event.m_x > 16)
	{
		if (event.m_y < 0)
		{
			curAddress -= align;
			Refresh();
		}
		else if (event.m_y > rc.height)
		{
			curAddress += align;
			Refresh();
		}
		else
			OnMouseDownL(event);
	}

	event.Skip();
}

void CMemoryView::OnMouseUpL(wxMouseEvent& event)
{
	if (event.m_x > 16)
	{
		curAddress = YToAddress(event.m_y);
		selecting = false;
		Refresh();
	}

	event.Skip();
}

void CMemoryView::OnScrollWheel(wxMouseEvent& event)
{
	const bool scroll_down = (event.GetWheelRotation() < 0);
	const int num_lines = event.GetLinesPerAction();

	if (scroll_down)
	{
		curAddress += num_lines;
	}
	else
	{
		curAddress -= num_lines;
	}

	Refresh();
	event.Skip();
}

void CMemoryView::OnPopupMenu(wxCommandEvent& event)
{
#if wxUSE_CLIPBOARD
	wxTheClipboard->Open();
#endif

	switch (event.GetId())
	{
#if wxUSE_CLIPBOARD
	case IDM_COPYADDRESS:
		wxTheClipboard->SetData(new wxTextDataObject(wxString::Format("%08x", selection)));
		break;

	case IDM_COPYHEX:
	{
		char temp[24];
		sprintf(temp, "%08x", debugger->ReadExtraMemory(memory, selection));
		wxTheClipboard->SetData(new wxTextDataObject(StrToWxStr(temp)));
	}
		break;
#endif

	case IDM_TOGGLEMEMORY:
		memory ^= 1;
		Refresh();
		break;

	case IDM_VIEWASFP:
		viewAsType = VIEWAS_FP;
		Refresh();
		break;

	case IDM_VIEWASASCII:
		viewAsType = VIEWAS_ASCII;
		Refresh();
		break;
	case IDM_VIEWASHEX:
		viewAsType = VIEWAS_HEX;
		Refresh();
		break;
	}

#if wxUSE_CLIPBOARD
	wxTheClipboard->Close();
#endif
	event.Skip();
}

void CMemoryView::OnMouseDownR(wxMouseEvent& event)
{
	// popup menu
	wxMenu* menu = new wxMenu;
	//menu.Append(IDM_GOTOINMEMVIEW, _("&Goto in mem view"));
#if wxUSE_CLIPBOARD
	menu->Append(IDM_COPYADDRESS, _("Copy &address"));
	menu->Append(IDM_COPYHEX, _("Copy &hex"));
#endif
	menu->Append(IDM_TOGGLEMEMORY, _("Toggle &memory"));

	wxMenu* viewAsSubMenu = new wxMenu;
	viewAsSubMenu->Append(IDM_VIEWASFP, _("FP value"));
	viewAsSubMenu->Append(IDM_VIEWASASCII, "ASCII");
	viewAsSubMenu->Append(IDM_VIEWASHEX, _("Hex"));
	menu->AppendSubMenu(viewAsSubMenu, _("View As:"));

	PopupMenu(menu);
}

void CMemoryView::OnPaint(wxPaintEvent& event)
{
	wxPaintDC dc(this);
	wxRect rc = GetClientRect();
	wxFont hFont("Courier");
	hFont.SetFamily(wxFONTFAMILY_TELETYPE);

	wxCoord w, h;
	dc.GetTextExtent("0WJyq", &w, &h, nullptr, nullptr, &hFont);
	if (h > rowHeight)
		rowHeight = h;
	dc.GetTextExtent("0WJyq", &w, &h, nullptr, nullptr, &DebuggerFont);
	if (h > rowHeight)
		rowHeight = h;

	if (viewAsType == VIEWAS_HEX)
		dc.SetFont(hFont);
	else
		dc.SetFont(DebuggerFont);

	dc.GetTextExtent("W", &w, &h);
	int fontSize = w;
	int textPlacement = 17 + 9 * fontSize;

	// TODO: Add any drawing code here...
	int width = rc.width;
	int numRows = (rc.height / rowHeight) / 2 + 2;
	dc.SetBackgroundMode(wxTRANSPARENT);
	const wxColour bgColor = *wxWHITE;
	wxPen nullPen(bgColor);
	wxPen currentPen(*wxBLACK_PEN);
	wxPen selPen(*wxGREY_PEN);
	nullPen.SetStyle(wxTRANSPARENT);

	wxBrush currentBrush(*wxLIGHT_GREY_BRUSH);
	wxBrush pcBrush(*wxGREEN_BRUSH);
	wxBrush mcBrush(*wxBLUE_BRUSH);
	wxBrush bgBrush(bgColor);
	wxBrush nullBrush(bgColor);
	nullBrush.SetStyle(wxTRANSPARENT);

	dc.SetPen(nullPen);
	dc.SetBrush(bgBrush);
	dc.DrawRectangle(0, 0, 16, rc.height);
	dc.DrawRectangle(0, 0, rc.width, 5 + 8);

	// TODO - clean up this freaking mess!!!!!
	for (int row = -numRows; row <= numRows; row++)
	{
		unsigned int address = curAddress + row * align;

		int rowY1 = rc.height / 2 + rowHeight * row - rowHeight / 2;
		int rowY2 = rc.height / 2 + rowHeight * row + rowHeight / 2;

		wxString temp = wxString::Format("%08x", address);
		u32 col = debugger->GetColor(address);
		wxBrush rowBrush(wxColour(col >> 16, col >> 8, col));
		dc.SetBrush(nullBrush);
		dc.SetPen(nullPen);
		dc.DrawRectangle(0, rowY1, 16, rowY2);

		if (selecting && (address == selection))
			dc.SetPen(selPen);
		else
			dc.SetPen(row == 0 ? currentPen : nullPen);

		if (address == debugger->GetPC())
			dc.SetBrush(pcBrush);
		else
			dc.SetBrush(rowBrush);

		dc.DrawRectangle(16, rowY1, width, rowY2 - 1);
		dc.SetBrush(currentBrush);
		dc.SetTextForeground("#600000"); // Dark red
		dc.DrawText(temp, 17, rowY1);

		if (viewAsType != VIEWAS_HEX)
		{
			char mem[256];
			debugger->GetRawMemoryString(memory, address, mem, 256);
			dc.SetTextForeground(wxTheColourDatabase->Find("NAVY"));
			dc.DrawText(StrToWxStr(mem), 17 + fontSize*(8), rowY1);
			dc.SetTextForeground(*wxBLACK);
		}

		if (debugger->IsAlive())
		{
			char dis[256] = { 0 };
			u32 mem_data = debugger->ReadExtraMemory(memory, address);

			if (viewAsType == VIEWAS_FP)
			{
				float flt = *(float *)(&mem_data);
				sprintf(dis, "f: %f", flt);
			}
			else if (viewAsType == VIEWAS_ASCII)
			{
				u32 a[4] = { (mem_data & 0xff000000) >> 24,
					(mem_data & 0xff0000) >> 16,
					(mem_data & 0xff00) >> 8,
					mem_data & 0xff };
				for (auto& word : a)
					if (word == '\0')
						word = ' ';
				sprintf(dis, "%c%c%c%c", a[0], a[1], a[2], a[3]);
			}
			else if (viewAsType == VIEWAS_HEX)
			{
				dis[0] = 0;
				dis[1] = 0;
				u32 mema[8] = {
					debugger->ReadExtraMemory(memory, address),
					debugger->ReadExtraMemory(memory, address + 4),
					debugger->ReadExtraMemory(memory, address + 8),
					debugger->ReadExtraMemory(memory, address + 12),
					debugger->ReadExtraMemory(memory, address + 16),
					debugger->ReadExtraMemory(memory, address + 20),
					debugger->ReadExtraMemory(memory, address + 24),
					debugger->ReadExtraMemory(memory, address + 28)
				};

				for (auto& word : mema)
				{
					char buf[32] = "";
					switch (dataType)
					{
					case 0:
						sprintf(buf, " %02X %02X %02X %02X",
							((word & 0xff000000) >> 24) & 0xFF,
							((word & 0xff0000) >> 16) & 0xFF,
							((word & 0xff00) >> 8) & 0xFF,
							word & 0xff);
						break;
					case 1:
						sprintf(buf, " %02X%02X %02X%02X",
							((word & 0xff000000) >> 24) & 0xFF,
							((word & 0xff0000) >> 16) & 0xFF,
							((word & 0xff00) >> 8) & 0xFF,
							word & 0xff);
						break;
					case 2:
						sprintf(buf, " %02X%02X%02X%02X",
							((word & 0xff000000) >> 24) & 0xFF,
							((word & 0xff0000) >> 16) & 0xFF,
							((word & 0xff00) >> 8) & 0xFF,
							word & 0xff);
						break;
					}
					strcat(dis, buf);
				}
				curAddress += 32;
			}
			else
			{
				sprintf(dis, "INVALID VIEWAS TYPE");
			}

			char desc[256] = "";
			if (viewAsType != VIEWAS_HEX)
				dc.DrawText(StrToWxStr(dis), textPlacement + fontSize*(8 + 8), rowY1);
			else
				dc.DrawText(StrToWxStr(dis), textPlacement, rowY1);

			if (desc[0] == 0)
				strcpy(desc, debugger->GetDescription(address).c_str());

			dc.SetTextForeground(*wxBLUE);

			if (strlen(desc))
				dc.DrawText(StrToWxStr(desc), 17 + fontSize*((8 + 8 + 8 + 30) * 2), rowY1);

			// Show blue memory check dot
			if (debugger->IsMemCheck(address))
			{
				dc.SetBrush(mcBrush);
				dc.DrawRectangle(8, rowY1 + 1, 11, 11);
			}
		}
	}

	dc.SetPen(currentPen);
}

void CMemoryView::OnResize(wxSizeEvent& event)
{
	Refresh();
	event.Skip();
}
