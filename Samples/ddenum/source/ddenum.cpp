//-----------------------------------------------------------------------------
// File: ddenum.cpp
//
// Desc: This sample demonstrates how to enumerate all of the devices and show
//       the driver information about each.
//
//
// Copyright (c) 1998-1999 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#define INITGUID

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

//-----------------------------------------------------------------------------
// Include files
//-----------------------------------------------------------------------------
#include <windows.h>

#include "resource.h"
#include <ddraw.h>
#include <stdarg.h>
#include <stdio.h>

#if (_MSC_VER >= 1900)
// Symbol is never tested for nullness
#pragma warning(disable : 26429)

// Symbol is not tested for nullness
#pragma warning(disable : 26430)

// Prefer to use gsl::at() instead of unchecked subscript
#pragma warning(disable : 26446)

// Only index into arrays using constant expressions
#pragma warning(disable : 26482)

// No array to pointer decay
#pragma warning(disable : 26485)

// Don't use reinterpret_cast
#pragma warning(disable : 26490)

// Don't use C-style variable arguments
#pragma warning(disable : 26826)

// Inconsistent annotation for 'WinMain'
#pragma warning(disable : 28251)
#endif

//-----------------------------------------------------------------------------
// Local definitions
//-----------------------------------------------------------------------------
#define NAME "DDEnum"
#define TITLE "DirectDraw Enumeration Example"

//-----------------------------------------------------------------------------
// Default settings
//-----------------------------------------------------------------------------
#define MAX_DEVICES 16

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------
int g_iMaxDevices = 0;
struct {
	DDDEVICEIDENTIFIER2 DeviceInfo;
	DDDEVICEIDENTIFIER2 DeviceInfoHost;
} g_DeviceInfo[MAX_DEVICES];

//-----------------------------------------------------------------------------
// Name: InitFail()
// Desc: This function is called if an initialization function fails
//-----------------------------------------------------------------------------
static void InitFail(LPCTSTR szError, ...)
{
	char szBuff[128];
	va_list vl;

	va_start(vl, szError);
	vsprintf(szBuff, szError, vl);
	MessageBoxA(NULL, szBuff, TITLE, MB_OK);
	va_end(vl);
}

//-----------------------------------------------------------------------------
// Name: SetInfoDlgText()
// Desc: Update all of the text and buttons in the dialog
//-----------------------------------------------------------------------------
static void SetInfoDlgText(HWND hDlg, int iCurrent, DWORD dwHost)
{
	char szBuff[128];
	GUID* pGUID;
	LPDDDEVICEIDENTIFIER2 pDI;

	if (dwHost == DDGDI_GETHOSTIDENTIFIER)
		CheckRadioButton(
			hDlg, IDC_RADIO_DEVICE, IDC_RADIO_HOST, IDC_RADIO_HOST);
	else
		CheckRadioButton(
			hDlg, IDC_RADIO_DEVICE, IDC_RADIO_DEVICE, IDC_RADIO_DEVICE);

	pDI = &g_DeviceInfo[iCurrent].DeviceInfo;
	if (dwHost == DDGDI_GETHOSTIDENTIFIER)
		pDI = &g_DeviceInfo[iCurrent].DeviceInfoHost;

	wsprintf(szBuff, "Device information for device %d of %d", iCurrent + 1,
		g_iMaxDevices);
	SetDlgItemTextA(hDlg, IDC_RADIO_DEVICE, szBuff);

	// Device ID stuff:
	wsprintf(szBuff, "%08X", pDI->dwVendorId);
	SetDlgItemTextA(hDlg, IDC_DWVENDORID, szBuff);
	wsprintf(szBuff, "%08X", pDI->dwDeviceId);
	SetDlgItemTextA(hDlg, IDC_DWDEVICEID, szBuff);
	wsprintf(szBuff, "%08X", pDI->dwSubSysId);
	SetDlgItemTextA(hDlg, IDC_DWSUBSYS, szBuff);
	wsprintf(szBuff, "%08X", pDI->dwRevision);
	SetDlgItemTextA(hDlg, IDC_DWREVISION, szBuff);

	// Driver version:
	wsprintf(szBuff, "%d.%02d.%02d.%04d",
		HIWORD(pDI->liDriverVersion.u.HighPart),
		LOWORD(pDI->liDriverVersion.u.HighPart),
		HIWORD(pDI->liDriverVersion.u.LowPart),
		LOWORD(pDI->liDriverVersion.u.LowPart));
	SetDlgItemText(hDlg, IDC_VERSION, szBuff);

	// Device description and HAL filename
	SetDlgItemText(hDlg, IDC_DESCRIPTION, pDI->szDescription);
	SetDlgItemText(hDlg, IDC_FILENAME, pDI->szDriver);

	// Unique driver/device identifier:
	pGUID = &pDI->guidDeviceIdentifier;
	wsprintf(szBuff, "%08X-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X",
		pGUID->Data1, pGUID->Data2, pGUID->Data3, pGUID->Data4[0],
		pGUID->Data4[1], pGUID->Data4[2], pGUID->Data4[3], pGUID->Data4[4],
		pGUID->Data4[5], pGUID->Data4[6], pGUID->Data4[7]);
	SetDlgItemText(hDlg, IDC_GUID, szBuff);

	// WHQL Level
	wsprintf(szBuff, "%08x", pDI->dwWHQLLevel);
	SetDlgItemText(hDlg, IDC_STATIC_WHQLLEVEL, szBuff);

	// Change the state and style of the Prev and Next buttons if needed
	if (0 == iCurrent) {
		// The Prev button should be disabled
		SetFocus(GetDlgItem(hDlg, IDOK));
		SendDlgItemMessage(hDlg, IDC_PREV, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
		SendMessage(hDlg, DM_SETDEFID, IDOK, 0);
		EnableWindow(GetDlgItem(hDlg, IDC_PREV), FALSE);
	} else if (IsWindowEnabled(GetDlgItem(hDlg, IDC_PREV)) == FALSE)
		EnableWindow(GetDlgItem(hDlg, IDC_PREV), TRUE);

	if (iCurrent >= (g_iMaxDevices - 1)) {
		// The Next button should be disabled
		SetFocus(GetDlgItem(hDlg, IDOK));
		SendDlgItemMessage(hDlg, IDC_NEXT, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
		SendMessage(hDlg, DM_SETDEFID, IDOK, 0);
		EnableWindow(GetDlgItem(hDlg, IDC_NEXT), FALSE);
	} else if (IsWindowEnabled(GetDlgItem(hDlg, IDC_NEXT)) == FALSE)
		EnableWindow(GetDlgItem(hDlg, IDC_NEXT), TRUE);
}

//-----------------------------------------------------------------------------
// Name: InfoDlgProc()
// Desc: The dialog window proc
//-----------------------------------------------------------------------------
static LRESULT CALLBACK InfoDlgProc(
	HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM /* lParam */)
{
	static int iCurrent = 0;
	static DWORD dwHost = 0;

	switch (uMsg) {
	case WM_INITDIALOG:
		// Setup the first devices text
		SetInfoDlgText(hDlg, iCurrent, dwHost);
		return TRUE;
	case WM_COMMAND:
		switch (HIWORD(wParam)) {
		case BN_CLICKED:
			switch (LOWORD(wParam)) {
			case IDOK:
			case IDCANCEL:
				EndDialog(hDlg, TRUE);
				return TRUE;
			case IDC_PREV:
				// Show the previous device
				if (iCurrent)
					iCurrent--;
				SetInfoDlgText(hDlg, iCurrent, dwHost);
				break;
			case IDC_NEXT:
				// Show the next device
				if (iCurrent < g_iMaxDevices)
					iCurrent++;
				SetInfoDlgText(hDlg, iCurrent, dwHost);
				break;
			case IDC_RADIO_HOST:
				dwHost = DDGDI_GETHOSTIDENTIFIER;
				SetInfoDlgText(hDlg, iCurrent, dwHost);
				break;
			case IDC_RADIO_DEVICE:
				dwHost = 0;
				SetInfoDlgText(hDlg, iCurrent, dwHost);
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
// Name: DDEnumCallbackEx()
// Desc: This callback gets the information for each device enumerated
//-----------------------------------------------------------------------------
static BOOL WINAPI DDEnumCallbackEx(GUID* pGUID, LPSTR /* pDescription */,
	LPSTR /* pName */, LPVOID /* pContext */, HMONITOR /* hm */)
{
	LPDIRECTDRAW pDD;          // DD1 interface, used to get DD7 interface
	LPDIRECTDRAW7 pDD7 = NULL; // DirectDraw7 interface
	HRESULT hRet;

	// Create the main DirectDraw object
	hRet = DirectDrawCreate(pGUID, &pDD, NULL);
	if (hRet != DD_OK) {
		InitFail("DirectDrawCreate FAILED");
		return DDENUMRET_CANCEL;
	}

	// Fetch DirectDraw4 interface
	hRet =
		pDD->QueryInterface(IID_IDirectDraw7, reinterpret_cast<LPVOID*>(&pDD7));
	if (hRet != DD_OK) {
		InitFail("QueryInterface FAILED");
		return DDENUMRET_CANCEL;
	}

	// Get the device information and save it
	hRet =
		pDD7->GetDeviceIdentifier(&g_DeviceInfo[g_iMaxDevices].DeviceInfo, 0);
	hRet = pDD7->GetDeviceIdentifier(
		&g_DeviceInfo[g_iMaxDevices].DeviceInfoHost, DDGDI_GETHOSTIDENTIFIER);

	// Finished with the DirectDraw object, so release it
	if (pDD7)
		pDD7->Release();

	// Bump to the next open slot or finish the callbacks if full
	if (g_iMaxDevices < MAX_DEVICES)
		g_iMaxDevices++;
	else
		return DDENUMRET_CANCEL;
	return DDENUMRET_OK;
}

//-----------------------------------------------------------------------------
// Name: DDEnumCallback()
// Desc: Old style callback retained for backwards compatibility
//-----------------------------------------------------------------------------
static BOOL WINAPI DDEnumCallback(
	GUID* pGUID, LPSTR pDescription, LPSTR pName, LPVOID context)
{
	return (DDEnumCallbackEx(pGUID, pDescription, pName, context, NULL));
}

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program. Initializes everything and calls
//       DirectDrawEnumerateEx() to get all of the device info.
//-----------------------------------------------------------------------------
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
	LPSTR /* lpszCmdLine */, int /* nCmdShow */)
{
	LPDIRECTDRAWENUMERATEEX pDirectDrawEnumerateEx;

	// Do a GetModuleHandle and GetProcAddress in order to get the
	// DirectDrawEnumerateEx function
	HINSTANCE hDDrawDLL = GetModuleHandleA("DDRAW");
	if (!hDDrawDLL) {
		InitFail("LoadLibrary() FAILED");
		return -1;
	}
	pDirectDrawEnumerateEx = (LPDIRECTDRAWENUMERATEEX)GetProcAddress(
		hDDrawDLL, "DirectDrawEnumerateExA");
	if (pDirectDrawEnumerateEx) {
		pDirectDrawEnumerateEx(DDEnumCallbackEx, NULL,
			DDENUM_ATTACHEDSECONDARYDEVICES | DDENUM_DETACHEDSECONDARYDEVICES |
				DDENUM_NONDISPLAYDEVICES);
	} else {
		// Old DirectDraw, so do it the old way
		DirectDrawEnumerateA(DDEnumCallback, NULL);
	}

	if (0 == g_iMaxDevices) {
		InitFail("No devices to enumerate.");
		return -1;
	}

	// Bring up the dialog to show all the devices
	DialogBoxA(hInstance, MAKEINTRESOURCE(IDD_DRIVERINFO), GetDesktopWindow(),
		(DLGPROC)InfoDlgProc);

	return 0;
}
