// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#define STRICT 1
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <time.h>
#include <fstream>
#include <commctrl.h>
#include "Resource.h"
#include "Orbiter.h"
#include "Launchpad.h"
#include "TabScenario.h"
#include "TabParam.h"
#include "TabVisual.h"
#include "TabModule.h"
#include "TabVideo.h"
#include "TabJoystick.h"
#include "TabExtra.h"
#include "TabAbout.h"
#include "Config.h"
#include "Log.h"
#include "Util.h"
#include "cryptstring.h"
#include "Help.h"
#include "Memstat.h"

using namespace std;

//=============================================================================
// Name: class MainDialog
// Desc: Handles the startup dialog ("Launchpad")
//=============================================================================

static MainDialog *g_pDlg = 0;
static time_t time0 = 0;
static UINT timerid = 0;

const DWORD dlgcol = 0xF0F4F8; // main dialog background colour

static int mnubt[] = {
	IDC_MNU_SCN, IDC_MNU_PRM, IDC_MNU_VIS, IDC_MNU_MOD,
	IDC_MNU_VID, IDC_MNU_JOY, IDC_MNU_EXT, IDC_MNU_ABT
};

//-----------------------------------------------------------------------------
// Name: MainDialog()
// Desc: This is the constructor for MainDialog
//-----------------------------------------------------------------------------
MainDialog::MainDialog (Orbiter *app)
{
	hDlg    = NULL;
	hInst   = app->GetInstance();
	ntab    = 0;
	pApp    = app;
	pCfg    = app->Cfg();
	g_pDlg  = this; // for nonmember callbacks
	CTab    = NULL;

	hDlgBrush = CreateSolidBrush (dlgcol);
	hShadowImg = LoadImage (hInst, MAKEINTRESOURCE(IDB_SHADOW), IMAGE_BITMAP, 0, 0, 0);

}

//-----------------------------------------------------------------------------
// Name: ~MainDialog()
// Desc: This is the destructor for MainDialog
//-----------------------------------------------------------------------------
MainDialog::~MainDialog ()
{
	int i;

	if (ntab) {
		for (i = 0; i < ntab; i++) delete Tab[i];
		delete []Tab;
		delete []pagidx;
		delete []tabidx;
	}
	DestroyWindow (hWait);
	DeleteObject (hDlgBrush);
	DeleteObject (hShadowImg);
}

//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Creates the main application dialog
//-----------------------------------------------------------------------------
HWND MainDialog::Create (bool startvideotab)
{
	if (!hDlg) {
		CreateDialog (hInst, MAKEINTRESOURCE(IDD_MAIN), NULL, AppDlgProc);
		AddTab (new ScenarioTab (this)); TRACENEW
		AddTab (new ParameterTab (this)); TRACENEW
		AddTab (new VisualTab (this)); TRACENEW
		AddTab (new ModuleTab (this)); TRACENEW
		AddTab (new DefVideoTab (this)); TRACENEW
		AddTab (new JoystickTab (this)); TRACENEW
		AddTab (pExtra = new ExtraTab (this)); TRACENEW
		AddTab (new AboutTab (this)); TRACENEW
		if (ntab) {
			pagidx  = new int[ntab]; TRACENEW
			tabidx  = new int[ntab]; TRACENEW
			for (int i = 0; i < ntab; i++) pagidx[i] = tabidx[i] = i;
		}
		InitTabControl (hDlg);
		InitSize (hDlg);
		SwitchTabPage (hDlg, 0);
		SetFocus (GetDlgItem (GetTabWindow(0), IDC_SCN_LIST)); // MOVE TO TAB CLASS!
		if (pCfg->CfgDemoPrm.bDemo) {
			SetDemoMode ();
			time0 = time (NULL);
			timerid = SetTimer (hDlg, 1, 1000, NULL);
		}
		Resize (hDlg, client0.right, client0.bottom, SIZE_RESTORED);
		HidePage (PG_VID); // no video options by default
		if (pCfg->rLaunchpad.right > pCfg->rLaunchpad.left) {
			RECT dr, lr = pCfg->rLaunchpad;
			int x = lr.left, y = lr.top, w = lr.right-lr.left, h = lr.bottom-lr.top;
			GetWindowRect (GetDesktopWindow(), &dr);
			x = min (max (x, dr.left), dr.right-w);
			y = min (max (y, dr.top), dr.bottom-h);
			SetWindowPos (hDlg, 0, x, y, w, h, 0);
		}
		char cbuf[256];
		strcpy(cbuf, uscram(SIG4));
		strcat(cbuf, "  \r\n");
		strcat (cbuf, uscram(SIG2));
		strcat (cbuf, "  \r\n");
		strcat (cbuf, uscram(SIG1AA));
		strcat (cbuf, "  \r\n");
		strcat (cbuf, uscram(SIG1AB));
		strcat (cbuf, "  ");
		SetWindowText (GetDlgItem (hDlg, IDC_BLACKBOX), cbuf);
		SetWindowText (GetDlgItem (hDlg, IDC_VERSION), uscram(SIG7));
		ShowWindow (hDlg, SW_SHOW);
		if (startvideotab) {
			UnhidePage (4, "Video");
			SwitchTabPage (hDlg, 4);
		}
	} else
		SwitchTabPage (hDlg, 0);

	return hDlg;
}

//-----------------------------------------------------------------------------
// Name: AddTab()
// Desc: Inserts a new tab into the list
//-----------------------------------------------------------------------------
void MainDialog::AddTab (LaunchpadTab *tab)
{
	LaunchpadTab **tmp = new LaunchpadTab*[ntab+1]; TRACENEW
	if (ntab) {
		memcpy (tmp, Tab, ntab*sizeof(LaunchpadTab*));
		delete []Tab;
	}
	Tab = tmp;
	Tab[ntab++] = tab;
}

const HWND MainDialog::GetTabWindow (int i) const
{
	return (i < ntab ? Tab[i]->TabWnd() : NULL);
}

//-----------------------------------------------------------------------------
// Name: InitTabControl()
// Desc: Sets up the tabs for the tab control interface
//-----------------------------------------------------------------------------
void MainDialog::InitTabControl (HWND hWnd)
{
	for (int i = 0; i < ntab; i++) {
		Tab[i]->Create();
		Tab[i]->GetConfig (pCfg);
	}
	hWait = CreateDialog (hInst, MAKEINTRESOURCE(IDD_PAGE_WAIT2), hWnd, WaitPageProc);
}

//-----------------------------------------------------------------------------
// Name: EnableLaunchButton()
// Desc: Enable/disable "Launch Orbiter" button
//-----------------------------------------------------------------------------
void MainDialog::EnableLaunchButton (bool enable) const
{
	EnableWindow (GetDlgItem (hDlg, IDLAUNCH), enable ? TRUE:FALSE);
}

void MainDialog::InitSize (HWND hWnd)
{
	RECT r, rl;
	GetClientRect (hWnd, &client0);
	GetClientRect (GetDlgItem (hWnd, IDC_BLACKBOX), &copyr0);
	GetClientRect (GetDlgItem (hWnd, IDC_SHADOW), &r);
	shadowh = r.bottom;
	r_launch0 = GetClientPos (hWnd, GetDlgItem (hWnd, IDLAUNCH));
	r_help0   = GetClientPos (hWnd, GetDlgItem (hWnd, 9));
	r_exit0   = GetClientPos (hWnd, GetDlgItem (hWnd, IDEXIT));
	r_wait0   = GetClientPos (hWnd, hWait);
	r_data0   = GetClientPos (hWnd, GetTabWindow(0));
	r_version0= GetClientPos (hWnd, GetDlgItem (hWnd, IDC_VERSION));

	r = GetClientPos (hDlg, GetDlgItem (hDlg, IDC_MNU_SCN));
	int y0 = r.top;
	r = GetClientPos (hDlg, GetDlgItem (hDlg, IDC_MNU_PRM));
	dy_bt = r.top - y0;

	GetClientRect (GetDlgItem (hWnd, IDC_LOGO), &rl);
	if (rl.bottom != copyr0.bottom) {
		SetWindowPos (GetDlgItem (hWnd, IDC_LOGO), NULL, 0, 0,
			client0.right-copyr0.right, copyr0.bottom,
			SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_NOCOPYBITS);
	}
}

BOOL MainDialog::Resize (HWND hWnd, DWORD w, DWORD h, DWORD mode)
{
	if (mode == SIZE_MINIMIZED) return TRUE;

	int i, w4, h4;
	int dw = (int)w - (int)client0.right;   // width change compared to initial size
	int dh = (int)h - (int)client0.bottom;  // height change compared to initial size
	int xb1 = r_launch0.left, xb2, xb3;
	int bg = r_exit0.left - r_help0.right;  // button gap
	int wb1 = r_launch0.right-r_launch0.left;
	int wb2 = r_help0.right-r_help0.left;
	int wb3 = r_exit0.right-r_exit0.left;
	int ww = wb1+wb2+wb3;
	int wf = r_exit0.right-r_launch0.left+dw-2*bg;
	if (wf < ww) { // shrink buttons
		wb1 = (wb1*wf)/ww;
		wb2 = (wb2*wf)/ww;
		wb3 = (wb3*wf)/ww;
		xb2 = xb1 + wb1 + bg;
		xb3 = xb2 + wb2 + bg;
	} else {
		xb2 = r_help0.left + dw;
		xb3 = r_exit0.left + dw;
	}
	int bh = r_exit0.bottom - r_exit0.top;  // button height

	SetWindowPos (GetDlgItem (hWnd, IDC_BLACKBOX), NULL,
		0, 0, copyr0.right + dw, copyr0.bottom,
		SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_NOCOPYBITS);
	SetWindowPos (GetDlgItem (hWnd, IDC_SHADOW), NULL,
		0, 0, w, shadowh,
		SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
	w4 = max (10, r_data0.right - r_data0.left + dw);
	h4 = max (10, r_data0.bottom - r_data0.top + dh);
	SetWindowPos (GetDlgItem (hWnd, IDLAUNCH), NULL,
		xb1, r_launch0.top+dh, wb1, bh,
		SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_NOCOPYBITS);
	SetWindowPos (GetDlgItem (hWnd, 9), NULL,
		xb2, r_help0.top+dh, wb2, bh,
		SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_NOCOPYBITS);
	SetWindowPos (GetDlgItem (hWnd, IDEXIT), NULL,
		xb3, r_exit0.top+dh, wb3, bh,
		SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_NOCOPYBITS);
	SetWindowPos (hWait, NULL,
		(w-(r_wait0.right-r_wait0.left))/2, max (r_wait0.top, r_wait0.top+(h-r_wait0.bottom)/2), 0, 0,
		SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOOWNERZORDER|SWP_NOZORDER);
	SetWindowPos (GetDlgItem (hWnd, IDC_VERSION), NULL,
		r_version0.left, r_version0.top+dh, 0, 0,
		SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_NOCOPYBITS|SWP_NOSIZE);
	for (i = 0; i < ntab; i++) {
		HWND hTab = GetTabWindow(i);
		if (hTab) SetWindowPos (hTab, NULL, 0, 0, w4, h4,
			SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
	}
	return FALSE;
}

//-----------------------------------------------------------------------------
// Name: SetDemoMode()
// Desc: Set launchpad controls into demo mode
//-----------------------------------------------------------------------------

void MainDialog::SetDemoMode ()
{
	//EnableWindow (GetDlgItem (hDlg, IDC_MAINTAB), FALSE);
	//ShowWindow (GetDlgItem (hDlg, IDC_MAINTAB), FALSE);

	static int hide_mnu[] = {
		IDC_MNU_PRM, IDC_MNU_VIS, IDC_MNU_MOD,
		IDC_MNU_VID, IDC_MNU_JOY, IDC_MNU_EXT,
	};
	for (int i = 0; i < 6; i++) EnableWindow (GetDlgItem (hDlg, hide_mnu[i]), FALSE);
	if (pCfg->CfgDemoPrm.bBlockExit) EnableWindow (GetDlgItem (hDlg, IDEXIT), FALSE);
}

//-----------------------------------------------------------------------------
// Name: UpdateConfig()
// Desc: Save current dialog settings in configuration
//-----------------------------------------------------------------------------
void MainDialog::UpdateConfig ()
{
	for (int i = 0; i < ntab; i++)
		Tab[i]->SetConfig (pCfg);

	// get launchpad window geometry (if not minimised)
	if (!IsIconic(hDlg))
		GetWindowRect (hDlg, &pCfg->rLaunchpad);
}

//-----------------------------------------------------------------------------
// Name: DlgProc()
// Desc: Message callback function for main dialog
//-----------------------------------------------------------------------------
BOOL MainDialog::DlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char cbuf[256];

	switch (uMsg) {
	case WM_INITDIALOG:
		hDlg = hWnd;
		return FALSE;
	case WM_CLOSE:
		if (pCfg->CfgDemoPrm.bBlockExit) return TRUE;
		UpdateConfig ();
		DestroyWindow (hWnd);
		return TRUE;
	case WM_DESTROY:
		if (pCfg->CfgDemoPrm.bDemo && timerid) {
			KillTimer (hWnd, 1);
			timerid = 0;
		}
		PostQuitMessage (0);
		return TRUE;
	case WM_SIZE:
		return Resize (hWnd, LOWORD(lParam), HIWORD(lParam), wParam);
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDLAUNCH:
			if (((ScenarioTab*)Tab[0])->GetSelScenario (cbuf, 256) == 1) {
				UpdateConfig ();
				pApp->Launch (cbuf);
			}
			return TRUE;
		case IDEXIT:
			PostMessage (hWnd, WM_CLOSE, 0, 0);
			return TRUE;
		case IDHELP:
			if (CTab) CTab->OpenHelp ();
			return TRUE;
		case IDC_MNU_SCN:
			SwitchTabPage (hWnd, PG_SCN);
			return TRUE;
		case IDC_MNU_PRM:
			SwitchTabPage (hWnd, PG_OPT);
			return TRUE;
		case IDC_MNU_VIS:
			SwitchTabPage (hWnd, PG_VIS);
			return TRUE;
		case IDC_MNU_MOD:
			SwitchTabPage (hWnd, PG_MOD);
			return TRUE;
		case IDC_MNU_VID:
			SwitchTabPage (hWnd, PG_VID);
			return TRUE;
		case IDC_MNU_JOY:
			SwitchTabPage (hWnd, PG_JOY);
			return TRUE;
		case IDC_MNU_EXT:
			SwitchTabPage (hWnd, PG_EXT);
			return TRUE;
		case IDC_MNU_ABT:
			SwitchTabPage (hWnd, PG_ABT);
			return TRUE;
		}
		break;
	case WM_SHOWWINDOW:
		if (pCfg->CfgDemoPrm.bDemo) {
			if (wParam) {
				time0 = time (NULL);
				if (!timerid) timerid = SetTimer (hWnd, 1, 1000, NULL);
			} else {
				if (timerid) {
					KillTimer (hWnd, 1);
					timerid = 0;
				}
			}
		}
		return 0;
	case WM_CTLCOLORSTATIC:
		if (lParam == (LPARAM)GetDlgItem (hWnd, IDC_BLACKBOX)) {
			HDC hDC = (HDC)wParam;
			SetTextColor (hDC, 0xF0B0B0);
			SetBkColor (hDC,0);
			//break;
			return (LRESULT)(HBRUSH)GetStockObject(BLACK_BRUSH);
		} else break;
	case WM_DRAWITEM: {
		LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
		if (wParam == IDC_SHADOW) {
			HDC hDC = lpDrawItem->hDC;
			HDC mDC = CreateCompatibleDC (hDC);
			HANDLE hp = SelectObject (mDC, hShadowImg);
			StretchBlt (hDC, 0, 0, lpDrawItem->rcItem.right, lpDrawItem->rcItem.bottom, 
				mDC, 0, 0, 8, 8, SRCCOPY);
			SelectObject (mDC, hp);
			DeleteDC (mDC);
			return TRUE;
		}
		} break;
	case WM_MOUSEMOVE:
		if (pCfg->CfgDemoPrm.bDemo) time0 = time(NULL); // reset timer
		break;
	case WM_KEYDOWN:
		if (pCfg->CfgDemoPrm.bDemo) time0 = time(NULL); // reset timer
		break;
	case WM_CTLCOLORDLG:
		return (LRESULT)hDlgBrush;
	case WM_TIMER:
		if (difftime (time (NULL), time0) > pCfg->CfgDemoPrm.LPIdleTime) { // auto-launch a demo
			if (SelectDemoScenario ())
				PostMessage (hWnd, WM_COMMAND, IDLAUNCH, 0);
		}
		return 0;
	}
	return FALSE;
}

//-----------------------------------------------------------------------------
// Name: WaitProc()
// Desc: Message callback function for wait page
//-----------------------------------------------------------------------------
BOOL MainDialog::WaitProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		SendDlgItemMessage (hWnd, IDC_PROGRESS1, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
		return TRUE;
	case WM_CTLCOLORSTATIC:
		if (lParam == (LPARAM)GetDlgItem (hWnd, IDC_WAITTEXT)) {
			HDC hDC = (HDC)wParam;
			//SetTextColor (hDC, 0xFFD0D0);
			//SetBkColor (hDC,0);
			SetBkMode (hDC, TRANSPARENT);
			return (LRESULT)hDlgBrush;
		} else break;
	case WM_CTLCOLORDLG:
		return (LRESULT)hDlgBrush;
	}
    return FALSE;
}

//-----------------------------------------------------------------------------
// Name: SwitchTabPage()
// Desc: Display a new page
//-----------------------------------------------------------------------------
void MainDialog::SwitchTabPage (HWND hWnd, int cpg)
{
	int idx = cpg;
	for (int pg = 0; pg < ntab; pg++)
		if (pg != cpg) Tab[pg]->Hide();
	CTab = Tab[cpg];
	CTab->Show();
}

void MainDialog::ShowWaitPage (HWND hWnd, bool show, long mem_committed)
{
	int pg, i;
	int showtab = (show ? SW_HIDE:SW_SHOW);
	int item[3] = {IDLAUNCH, 9, IDEXIT};

	if (show) {
		SetCursor(LoadCursor(NULL, IDC_WAIT));
		for (pg = 0; pg < ntab; pg++) Tab[pg]->Hide();
		for (i = 0; i < ntab; i++) Tab[i]->Hide();
		mem_wait = mem_committed/1000;
		mem0 = pApp->memstat->HeapUsage();
		SendDlgItemMessage (hWait, IDC_PROGRESS1, PBM_SETPOS, 0, 0);
		ShowWindow (GetDlgItem (hWait, IDC_PROGRESS1), mem_wait ? SW_SHOW:SW_HIDE);
		ShowWindow (hWait, SW_SHOW);
	} else {
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		ShowWindow (hWait, SW_HIDE);
		SwitchTabPage (hWnd, 0);
	}
	for (i = 0; i < 3; i++)
		ShowWindow (GetDlgItem (hWnd, item[i]), showtab);
	for (i = 0; i < ntab; i++)
		if (tabidx[i] >= 0)
			ShowWindow (GetDlgItem (hWnd, mnubt[i]), showtab);
}

void MainDialog::UpdateWaitProgress ()
{
	if (mem_wait) {
		long mem = pApp->memstat->HeapUsage();
		SendDlgItemMessage (hWait, IDC_PROGRESS1, PBM_SETPOS, (mem0-mem)/mem_wait, 0);
	}
}

void MainDialog::HidePage (int idx)
{
	if (tabidx[idx] < 0) return; // already hidden

	ShowWindow (GetDlgItem (hDlg, mnubt[tabidx[idx]]), SW_HIDE);
	int i;
	RECT r;

	tabidx[idx] = -1;
	for (i = idx+1; i < ntab; i++) {
		tabidx[i]--;
		r = GetClientPos (hDlg, GetDlgItem (hDlg, mnubt[i]));
		r.top -= dy_bt, r.bottom -= dy_bt;
		SetClientPos (hDlg, GetDlgItem (hDlg, mnubt[i]), r);
	}

	for (i = 0; i < ntab; i++)
		if (tabidx[i] >= 0)
			pagidx[tabidx[i]] = i;

	InvalidateRect (hDlg, NULL, TRUE);
}

void MainDialog::UnhidePage (int idx, char *tab)
{
	int i, j;
	RECT r;

	for (i = 0; i < ntab; i++) {
		if (pagidx[i] == idx) return; // page already present
		if (pagidx[i] > idx) break;
	}
	for (j = ntab-2; j >= i; j--)
		pagidx[j+1] = pagidx[j];
	pagidx[i] = idx;

	for (i = 0; i < ntab; i++)
		tabidx[pagidx[i]] = i;

	//TC_ITEM tie;
	//tie.mask = TCIF_TEXT;
	//tie.iImage = -1;
	//tie.pszText = tab;

	for (i = ntab-1; i > idx; i--) {
		r = GetClientPos (hDlg, GetDlgItem (hDlg, mnubt[i]));
		r.top += dy_bt, r.bottom += dy_bt;
		SetClientPos (hDlg, GetDlgItem (hDlg, mnubt[i]), r);
	}
	ShowWindow (GetDlgItem (hDlg, mnubt[tabidx[idx]]), SW_SHOW);
	//TabCtrl_InsertItem (GetDlgItem (hDlg, IDC_MAINTAB), tabidx[idx], &tie);
	// TO BE DONE!

	//InvalidateRect (hDlg, NULL, TRUE);
}

//-----------------------------------------------------------------------------
// Name: GetDemoScenario()
// Desc: returns the name of an arbitrary scenario in the demo folder
//-----------------------------------------------------------------------------
int MainDialog::SelectDemoScenario ()
{
	char cbuf[256];
	HWND hTree = GetDlgItem (GetTabWindow(PG_SCN), IDC_SCN_LIST);
	HTREEITEM demo;
	TV_ITEM tvi;
	tvi.hItem = TreeView_GetRoot (hTree);
	tvi.pszText = cbuf;
	tvi.cchTextMax = 256;
	tvi.cChildren = 0;
	tvi.mask = TVIF_HANDLE | TVIF_TEXT | TVIF_CHILDREN;
	while (tvi.hItem) {
		TreeView_GetItem (hTree, &tvi);
		if (!_stricmp (cbuf, "Demo")) break;
		tvi.hItem = TreeView_GetNextSibling (hTree, tvi.hItem);
	}
	if (tvi.hItem) demo = tvi.hItem;
	else           return 0;

	int seldemo, ndemo = 0;
	tvi.hItem = TreeView_GetChild (hTree, demo);
	while (tvi.hItem) {
		TreeView_GetItem (hTree, &tvi);
		if (!tvi.cChildren) ndemo++;
		tvi.hItem = TreeView_GetNextSibling (hTree, tvi.hItem);
	}
	if (!ndemo) return 0;
	seldemo = (rand()*ndemo)/(RAND_MAX+1);
	ndemo = 0;
	tvi.hItem = TreeView_GetChild (hTree, demo);
	while (tvi.hItem) {
		TreeView_GetItem (hTree, &tvi);
		if (!tvi.cChildren) {
			if (ndemo == seldemo) {
				return (TreeView_SelectItem (hTree, tvi.hItem) != 0);
			}
			ndemo++;
		}
		tvi.hItem = TreeView_GetNextSibling (hTree, tvi.hItem);
	}
	return 0;
}

// ****************************************************************************
// "Extra Parameters" page
// ****************************************************************************

static char *desc_fixedstep = "Force Orbiter to advance the simulation by a fixed time interval in each frame.";

void OpenDynamics (HINSTANCE, HWND);

HTREEITEM MainDialog::RegisterExtraParam (LaunchpadItem *item, HTREEITEM parent)
{
	return pExtra->RegisterExtraParam (item, parent);
}

bool MainDialog::UnregisterExtraParam (LaunchpadItem *item)
{
	return pExtra->UnregisterExtraParam (item);
}

HTREEITEM MainDialog::FindExtraParam (const char *name, const HTREEITEM parent)
{
	return pExtra->FindExtraParam (name, parent);
}

void MainDialog::WriteExtraParams ()
{
	pExtra->WriteExtraParams ();
}

#ifdef DO_NETWORK_OLD
//-----------------------------------------------------------------------------
// Name: ConnectionsCallback()
// Desc: Callback function for connections enumeration
//-----------------------------------------------------------------------------
BOOL FAR PASCAL ConnectionsCallback (LPCGUID pguidSP, VOID* pConnection, 
    DWORD dwConnectionSize, LPCDPNAME pName, DWORD dwFlags, VOID* pvContext)
{
    HRESULT       hr;
    LPDIRECTPLAY4 pDP = NULL;
    VOID*         pConnectionBuffer = NULL;
    HWND          hWndListBox = (HWND)pvContext;
    LRESULT       iIndex;

    // Create a IDirectPlay object
    if( FAILED( hr = CoCreateInstance( CLSID_DirectPlay, NULL, CLSCTX_ALL, 
                                       IID_IDirectPlay4A, (VOID**)&pDP ) ) )
        return FALSE; // Error, stop enumerating
    
    // Test the if the connection is available by attempting to initialize 
    // the connection
    if( FAILED( hr = pDP->InitializeConnection( pConnection, 0 ) ) )
    {
		pDP->Release();
        return TRUE; // Unavailable connection, keep enumerating
    }

    // Don't need the IDirectPlay interface anymore, so release it
	pDP->Release();
    
    // Found a good connection, so put it in the listbox
    iIndex = SendMessage( hWndListBox, LB_ADDSTRING, 0, 
                          (LPARAM)pName->lpszShortNameA );
    if( iIndex == CB_ERR )
        return FALSE; // Error, stop enumerating

    pConnectionBuffer = new BYTE[ dwConnectionSize ]; TRACENEW
    if( pConnectionBuffer == NULL )
        return FALSE; // Error, stop enumerating

    // Store pointer to GUID in listbox
    memcpy( pConnectionBuffer, pConnection, dwConnectionSize );
    SendMessage( hWndListBox, LB_SETITEMDATA, iIndex, 
                 (LPARAM)pConnectionBuffer );

    return TRUE; // Keep enumerating
}

//-----------------------------------------------------------------------------
// Name: InitNetworkPage()
// Desc: Set multiplayer options
//-----------------------------------------------------------------------------
void MainDialog::InitNetworkPage ()
{
	int i;
	HWND hWndConn = GetDlgItem (hPage[PG_NET], IDC_LIST1);
	SetWindowText (GetDlgItem (hPage[PG_NET], IDC_EDIT1), pCfg->CfgMplayerPrm.mpName);
	SetWindowText (GetDlgItem (hPage[PG_NET], IDC_EDIT2), pCfg->CfgMplayerPrm.mpCallsign);
	pApp->GetDPlay()->GetDP()->EnumConnections (&pApp->GetGUID(), ConnectionsCallback, hWndConn, 0);
	i = SendDlgItemMessage (hPage[PG_NET], IDC_LIST1, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)pCfg->CfgMplayerPrm.mpConnection);
    SendDlgItemMessage(hPage[PG_NET], IDC_LIST1, LB_SETCURSEL, i != LB_ERR ? i:0, 0);
	ZeroMemory (&DPSIHead, sizeof (DPSessionInfo));
	DPSIHead.pDPSINext = &DPSIHead;
}

void MainDialog::Network_ShowPage1 ()
{
	ShowWindow (hPage[PG_NET2], SW_HIDE);
	ShowWindow (hPage[PG_NET3], SW_HIDE);
	ShowWindow (hPage[PG_NET], SW_SHOW);
}

void MainDialog::Network_ShowPage2 ()
{
	HRESULT hr;
	char cbuf[256];
	GetWindowText (GetDlgItem (hPage[PG_NET], IDC_EDIT1), cbuf, 256);
	if (!cbuf[0]) {
		MessageBeep (-1);
		return;
	}
	LRESULT idx = SendDlgItemMessage (hPage[PG_NET], IDC_LIST1, LB_GETCURSEL, 0, 0);
	VOID *pConnection = (VOID*)SendDlgItemMessage (hPage[PG_NET], IDC_LIST1, LB_GETITEMDATA, idx, 0);

	if (FAILED (pApp->GetDPlay()->Restart())) {
		LOGOUT_ERR("Could not create DirectPlay instance");
		return;
	}
	if (FAILED (hr = pApp->GetDPlay()->GetDP()->InitializeConnection (pConnection, 0))) {
		LOGOUT_DPERR(hr);
		return;
	}

	ShowWindow (hPage[PG_NET], SW_HIDE);
	ShowWindow (hPage[PG_NET3], SW_HIDE);
	ShowWindow (hPage[PG_NET2], SW_SHOW);
	SendDlgItemMessage (hPage[PG_NET2], IDC_LIST1, LB_RESETCONTENT, 0, 0);
	SendDlgItemMessage (hPage[PG_NET2], IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)"Click Create to start new game. Click Search to see a list of games.");
	bSessionSearch = false;
	SetDlgItemText (hPage[PG_NET2], IDC_NET_SEARCH, "Start Search");
	Network_InitSessionListbox();
}

void MainDialog::Network_ShowPage3 ()
{
	LPDPSESSIONDESC2 pdesc;
	HRESULT hr;
	if (FAILED (hr = pApp->GetDPlay()->GetSessionDesc (&pdesc))) {
		LOGOUT_DPERR(hr);
	} else {
		char cbuf[256];
		sprintf (cbuf, "ORBITER %s '%s'", pApp->GetDPlay()->isServer() ? "SERVER":"CLIENT",
			pdesc->lpszSessionNameA);
		SetWindowText (GetDlgItem (hPage[PG_NET3], IDC_STATIC1), cbuf);
	}

	ShowWindow (hPage[PG_NET], SW_HIDE);
	ShowWindow (hPage[PG_NET2], SW_HIDE);
	ShowWindow (hPage[PG_NET3], SW_SHOW);
}

HRESULT MainDialog::Network_CreateGame (HWND hWnd)
{
	char player_name[256], player_callsign[64];
	DPSESSIONDESC2 dpsd;
	DPNAME dpname;
	HRESULT hr;

	int res = DialogBox ((HINSTANCE)GetWindowLongPtr (hWnd, GWL_HINSTANCE),
		MAKEINTRESOURCE(IDD_NET_CREATE), hWnd, DlgCreateSessionProc);
	if (res == IDCANCEL) return S_OK;

	// Create a new session
	ZeroMemory (&dpsd, sizeof (dpsd));
	dpsd.dwSize           = sizeof (dpsd);
	dpsd.guidApplication  = pApp->AppGUID;
	dpsd.lpszSessionNameA = cSessionName;
	dpsd.dwMaxPlayers     = nSessionMaxPlayer;
	dpsd.dwFlags          = DPSESSION_KEEPALIVE /*| DPSESSION_MIGRATEHOST*/;

	if (FAILED (hr = pApp->GetDPlay()->GetDP()->Open (&dpsd, DPOPEN_CREATE))) {
		LOGOUT_DPERR (hr);
		return hr;
	} else
		LOGOUT1P("Server created for session '%s'", cSessionName);

	// Add the local player to the session
	GetWindowText (GetDlgItem (hPage[PG_NET], IDC_EDIT1), player_name, 256);
	GetWindowText (GetDlgItem (hPage[PG_NET], IDC_EDIT2), player_callsign, 64);
	ZeroMemory (&dpname, sizeof (DPNAME));
	dpname.dwSize = sizeof (DPNAME);
	dpname.lpszLongNameA = player_name;
	dpname.lpszShortNameA = player_callsign;
	if (FAILED (hr = pApp->GetDPlay()->CreateLocalPlayer (&dpname, true))) {
		LOGOUT_DPERR (hr);
		return hr;
	}
	pApp->GetDPlay()->RefreshPlayerList();
	pApp->bMultiplayer = TRUE;
	pApp->bServer = TRUE;
	Network_PlayerListChanged();
	LOGOUT1P ("Local player added to session: '%s'", player_name);

	Network_ShowPage3();

	return S_OK;
}

BOOL MainDialog::CreateSessionProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char cbuf[256];
	int clen;

	switch (uMsg) {
	case WM_INITDIALOG:
		GetWindowText (GetDlgItem (hPage[PG_NET], IDC_EDIT1), cbuf, 256);
		strcat (cbuf, "'s Orbiter Server");
		SetDlgItemText (hWnd, IDC_NET_SESSION, cbuf);
		SetDlgItemText (hWnd, IDC_NET_MAXPLAYER, "10");
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDOK:
			clen = GetDlgItemText (hWnd, IDC_NET_SESSION, cSessionName, 256);
			if (!clen) return TRUE; // don't accept blank sessio name
			GetDlgItemText (hWnd, IDC_NET_MAXPLAYER, cbuf, 256);
			if (!sscanf (cbuf, "%d", &nSessionMaxPlayer) || nSessionMaxPlayer < 1 ||
				nSessionMaxPlayer > 100) return TRUE;
			EndDialog (hWnd, IDOK);
			return TRUE;
		case IDCANCEL:
			EndDialog (hWnd, IDCANCEL);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

HRESULT MainDialog::Network_JoinGame (HWND hWnd)
{
	char player_name[256], player_callsign[64];
	HRESULT hr;
    DPSESSIONDESC2 dpsd;
    DPNAME         dpname;
	HWND hList = GetDlgItem (hWnd, IDC_LIST1);
    DPSessionInfo* pDPSISelected = NULL;
	int nItemSelected = SendMessage (hList, LB_GETCURSEL, 0, 0);

	// Add status text in list box
	pDPSISelected = (DPSessionInfo*) SendMessage (hList, LB_GETITEMDATA, nItemSelected, 0);
	if (pDPSISelected == NULL) {
		MessageBeep (-1);
		return S_OK;
	}

    // Setup the DPSESSIONDESC2, and get the session guid from 
    // the selected listbox item
    ZeroMemory (&dpsd, sizeof (dpsd));
    dpsd.dwSize          = sizeof (dpsd);
    dpsd.guidInstance    = pDPSISelected->guidSession;
    dpsd.guidApplication = pApp->AppGUID;

    // Join the session
    // **** g_bHostPlayer = FALSE;
    if (FAILED (hr = pApp->GetDPlay()->GetDP()->Open (&dpsd, DPOPEN_JOIN))) {
		LOGOUT_DPERR (hr);
        return hr;
	}

    // Create player based on g_strLocalPlayerName.  
    // Store the player's DPID in g_LocalPlayerDPID.
    // Also all DirectPlay messages for this player will signal g_hDPMessageEvent
	GetDlgItemText (hPage[PG_NET], IDC_EDIT1, player_name, 256);
	GetDlgItemText (hPage[PG_NET], IDC_EDIT2, player_callsign, 64);
    ZeroMemory (&dpname, sizeof (DPNAME));
    dpname.dwSize         = sizeof (DPNAME);
	dpname.lpszLongNameA = player_name;
	dpname.lpszShortNameA = player_callsign;

    if (FAILED (hr = pApp->GetDPlay()->CreateLocalPlayer (&dpname))) {
		LOGOUT_DPERR (hr);
        return hr;
	}
	pApp->GetDPlay()->RefreshPlayerList();
	pApp->bMultiplayer = TRUE;
	pApp->bServer = FALSE;
	Network_PlayerListChanged();
	LOGOUT1P ("Local player added to session: '%s'", player_name);

	Network_ShowPage3();

	return S_OK;
}

BOOL FAR PASCAL Network_EnumSessionsCallback (LPCDPSESSIONDESC2 pdpsd, DWORD *pdwTimeout,
	DWORD dwFlags, VOID *pvContext)
{
	MainDialog *md = (MainDialog*)pvContext;
	DPSessionInfo *pDPSINew = NULL;
	if (dwFlags & DPESC_TIMEDOUT) return FALSE;
	pDPSINew = new DPSessionInfo; TRACENEW
	if (pDPSINew == NULL) return FALSE;
	ZeroMemory (pDPSINew, sizeof (DPSessionInfo));
	pDPSINew->guidSession = pdpsd->guidInstance;
	sprintf (pDPSINew->szSession, "%s (%d/%d)", pdpsd->lpszSessionNameA,
		pdpsd->dwCurrentPlayers, pdpsd->dwMaxPlayers);
	pDPSINew->pDPSINext = md->DPSIHead.pDPSINext;
	md->DPSIHead.pDPSINext = pDPSINew;
	return TRUE;
}

HRESULT MainDialog::Network_SearchGames (HWND hWnd)
{
	HRESULT hr;
	HWND hList = GetDlgItem (hPage[PG_NET2], IDC_LIST1);
	DPSESSIONDESC2 dpsd;
	DPSessionInfo *pDPSISelected = NULL;
	GUID guidSelectedSession;

	bool bFindSelectedGUID = false;
	bool bFoundSelectedGUID = true;

	int nItemSelected = SendMessage (hList, LB_GETCURSEL, 0, 0);
	if (nItemSelected != LB_ERR) {
		pDPSISelected = (DPSessionInfo*)SendMessage (hList, LB_GETITEMDATA, nItemSelected, 0);
		if (pDPSISelected != NULL) {
			guidSelectedSession = pDPSISelected->guidSession;
			bFindSelectedGUID = true;
		}
	}

	// remove previous games from linked list
	Network_SessionsCleanup();

	// enum sessions and let DirectPlay decide the timeout
	ZeroMemory (&dpsd, sizeof (dpsd));
	dpsd.dwSize = sizeof (dpsd);
	dpsd.guidApplication = pApp->AppGUID;

	// enumerate all the active DirectPlay games on the selected connection
	hr = pApp->GetDPlay()->GetDP()->EnumSessions (&dpsd, 0, Network_EnumSessionsCallback, this,
		DPENUMSESSIONS_ALL | DPENUMSESSIONS_ASYNC);

	if (FAILED (hr)) {
		if (hr == DPERR_USERCANCEL) {
			if (bSessionSearch) {
				CheckDlgButton (hPage[PG_NET2], IDC_NET_SEARCH, BST_UNCHECKED);
				SendMessage (hPage[PG_NET2], WM_COMMAND, IDC_NET_SEARCH, 0);
			}
			return S_OK;
		} else {
			Network_InitSessionListbox ();
			if (hr == DPERR_CONNECTING)
				return S_OK;
			return hr;
		}
	}

	// Tell listbox not to redraw itself since the contents are going to change
	SendMessage (hList, WM_SETREDRAW, FALSE, 0);

	// Add the enumerated sessions to the listbox
	if (DPSIHead.pDPSINext != &DPSIHead) {

		// Clear the contents of the listbox and enable the join button
		SendMessage (hList, LB_RESETCONTENT, 0, 0);
		EnableWindow (GetDlgItem (hWnd, IDC_NET_JOIN), TRUE);

		DPSessionInfo *pDPSI = DPSIHead.pDPSINext;
		while (pDPSI != &DPSIHead) {
			int idx = SendMessage (hList, LB_ADDSTRING, 0, (LPARAM)pDPSI->szSession);
			SendMessage (hList, LB_SETITEMDATA, idx, (LPARAM)pDPSI);
			if (bFindSelectedGUID) {
				// Look for the session that was selected before
				if (pDPSI->guidSession == guidSelectedSession) {
					SendMessage (hList, LB_SETCURSEL, idx, 0);
					bFoundSelectedGUID = true;
				}
			}
			pDPSI = pDPSI->pDPSINext;
		}
		if (!bFindSelectedGUID || !bFoundSelectedGUID)
			SendMessage (hList, LB_SETCURSEL, 0, 0);
	} else {
		// There are no active sessions, so just reset the listbox
		Network_InitSessionListbox();
	}

	// Tell the list to redraw itself now
	SendMessage (hList, WM_SETREDRAW, TRUE, 0);
	InvalidateRect (hList, NULL, FALSE);
	
	return S_OK;
}

void MainDialog::Network_SessionsCleanup ()
{
	DPSessionInfo *pDPSI = DPSIHead.pDPSINext;
	DPSessionInfo *pDPSIDelete;

	while (pDPSI != &DPSIHead) {
		pDPSIDelete = pDPSI;
		pDPSI = pDPSI->pDPSINext;
		SAFE_DELETE (pDPSIDelete);
	}
	DPSIHead.pDPSINext = &DPSIHead;
}

void MainDialog::Network_InitSessionListbox ()
{
	HWND hList = GetDlgItem (hPage[PG_NET2], IDC_LIST1);
	SendMessage (hList, LB_RESETCONTENT, 0, 0);
	if (bSessionSearch)
		SendMessage (hList, LB_ADDSTRING, 0, (LPARAM)"Looking for games ...");
	else
		SendMessage (hList, LB_ADDSTRING, 0, (LPARAM)"Click Start Search to search for games. Click Create to start a new game.");
	SendMessage (hList, LB_SETITEMDATA, 0, NULL);
	SendMessage (hList, LB_SETCURSEL, 0, 0);

	// Disable the Join button until sessions are found
	EnableWindow (GetDlgItem (hPage[PG_NET2], IDC_NET_JOIN), FALSE);
}

void MainDialog::Network_PlayerListChanged ()
{
	char cbuf[256];
	HWND hList = GetDlgItem (hPage[PG_NET3], IDC_LIST1);
	SendMessage (hList, LB_RESETCONTENT, 0, 0);
	DPlayer *pdp = pApp->GetDPlay()->FirstPlayer();
	while (pdp) {
		sprintf (cbuf, "%s [%s]", pdp->name, pdp->call);
		SendMessage (hList, LB_ADDSTRING, 0, (LPARAM)cbuf);
		pdp = pdp->next;
	}
}
#endif // DO_NETWORK_OLD

//-----------------------------------------------------------------------------
// Name: RefreshScenarioList()
// Desc: Re-scan scenario list
//-----------------------------------------------------------------------------
void MainDialog::RefreshScenarioList ()
{
	((ScenarioTab*)Tab[0])->RefreshList();
}

//-----------------------------------------------------------------------------
// Name: SelRootScenario()
// Desc: select the given scenario
//-----------------------------------------------------------------------------
bool MainDialog::SelRootScenario (char *scn)
{
	return ((ScenarioTab*)Tab[0])->SelRootScenario (scn);
}


//=============================================================================
// Nonmember functions
//=============================================================================

//-----------------------------------------------------------------------------
// Name: AppDlgProc()
// Desc: Static msg handler which passes messages from the main dialog
//       to the application class.
//-----------------------------------------------------------------------------
INT_PTR CALLBACK AppDlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return g_pDlg->DlgProc (hWnd, uMsg, wParam, lParam);
}

//-----------------------------------------------------------------------------
// Name: WaitPageProc()
// Desc: Dummy function for wait page
//-----------------------------------------------------------------------------
INT_PTR CALLBACK WaitPageProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return g_pDlg->WaitProc (hWnd, uMsg, wParam, lParam);
}

LONG_PTR FAR PASCAL MsgProc_CopyrightFrame (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//switch (uMsg) {
	//case WM_PAINT:

	return DefWindowProc (hWnd, uMsg, wParam, lParam);
}